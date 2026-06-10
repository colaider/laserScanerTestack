/**
 * @file RobotServer.cpp
 * @brief Implementation of TCP/IP server for robot controller communication.
 *
 * This module provides the network communication layer between the measurement
 * system and external robot controllers. It implements a simple text-based
 * protocol over TCP/IP for command/response exchange.
 *
 * ## Network Architecture:
 * ```
 * ┌─────────────────┐         TCP/IP          ┌─────────────────┐
 * │ Robot Controller│◄───────────────────────►│  Scanner App    │
 * │   (Client)      │        Port 5010        │   (Server)      │
 * └─────────────────┘                         └─────────────────┘
 * ```
 *
 * ## Protocol Details:
 * - **Transport**: TCP (reliable, ordered delivery)
 * - **Port**: Configurable (typically 5010)
 * - **Message Format**: ASCII text terminated by newline or '>'
 * - **Encoding**: UTF-8 compatible (ASCII subset)
 *
 * ## Connection Lifecycle:
 * 1. Server binds to port and listens
 * 2. Robot connects as client
 * 3. Command/response exchange
 * 4. Robot disconnects or sends QUIT
 * 5. Server can accept new connection
 *
 * @version 1.0
 * @date 2026-02-05
 *
 * @see RobotServer.h for class declaration
 * @see RobotControlLoop() for protocol handling
 */

#include "RobotServer.h"

 // ===========================================================================
 // CONSTRUCTOR / DESTRUCTOR
 // ===========================================================================

 /**
  * @brief Constructs the server and initializes Winsock.
  *
  * Calls WSAStartup() to initialize the Windows Sockets 2.2 library.
  * This must succeed before any socket operations can be performed.
  *
  * @note Winsock version 2.2 is requested (MAKEWORD(2, 2)).
  * @note If initialization fails, _initialized remains false and
  *       all subsequent operations will fail gracefully.
  */
RobotServer::RobotServer() {
    WSADATA wsaData;

    // Initialize Winsock 2.2
    // MAKEWORD(2, 2) requests version 2.2 of the Winsock API
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        _initialized = true;
    }
    else {
        std::cerr << "[NET] Winsock Init Failed.\n";
    }
}

/**
 * @brief Destructor - cleans up socket and Winsock resources.
 *
 * Ensures proper cleanup by:
 * 1. Disconnecting any active client
 * 2. Calling WSACleanup() to release Winsock resources
 *
 * @note WSACleanup() is only called if WSAStartup() succeeded.
 */
RobotServer::~RobotServer() {
    Disconnect();

    // Only cleanup if we successfully initialized
    if (_initialized) {
        WSACleanup();
    }
}

// ===========================================================================
// CONNECTION MANAGEMENT
// ===========================================================================

/**
 * @brief Blocks until a robot controller connects on the specified port.
 *
 * Creates a listening socket, binds to the specified port, and waits
 * for an incoming TCP connection. This is a blocking call.
 *
 * ## Socket Setup Steps:
 * 1. Create TCP socket (AF_INET, SOCK_STREAM)
 * 2. Enable SO_REUSEADDR for quick reconnection
 * 3. Bind to specified port on all interfaces (INADDR_ANY)
 * 4. Listen with backlog of 1 (single client)
 * 5. Accept incoming connection
 * 6. Close listening socket (only one client supported)
 *
 * @param[in] port  TCP port number to listen on (e.g., 5010)
 *
 * @return true if a client connected successfully, false on error
 *
 * @note SO_REUSEADDR allows immediate port reuse after disconnect,
 *       which is essential for quick reconnection scenarios.
 * @note The listening socket is closed after accepting - only one
 *       client can connect per WaitForConnection() call.
 */
bool RobotServer::WaitForConnection(unsigned short port) {
    // Create TCP socket
    // AF_INET = IPv4, SOCK_STREAM = TCP
    SOCKET l = socket(AF_INET, SOCK_STREAM, 0);
    if (l == INVALID_SOCKET) {
        return false;
    }

    // -----------------------------------------------------------------------
    // Enable SO_REUSEADDR for quick reconnection
    // -----------------------------------------------------------------------
    // Without this, the port remains in TIME_WAIT state for ~60 seconds
    // after disconnect, preventing immediate rebind. Essential for
    // scenarios where robot disconnects and reconnects quickly.
    int opt = 1;
    setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // -----------------------------------------------------------------------
    // Bind to port on all network interfaces
    // -----------------------------------------------------------------------
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on all interfaces
    a.sin_port = htons(port);                // Convert port to network byte order

    if (bind(l, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) {
        std::cerr << "[NET] Bind failed.\n";
        closesocket(l);
        return false;
    }

    // -----------------------------------------------------------------------
    // Start listening and accept connection
    // -----------------------------------------------------------------------
    // Backlog of 1 - we only support a single client
    listen(l, 1);
    std::cout << "Waiting for Robot on port " << port << "...\n";

    // Block until client connects
    _socket = accept(l, nullptr, nullptr);

    // Close listening socket - we only accept one client
    closesocket(l);

    if (_socket != INVALID_SOCKET) {
        std::cout << "Robot Connected!\n";
        return true;
    }
    return false;
}

// ===========================================================================
// MESSAGE EXCHANGE
// ===========================================================================

/**
 * @brief Reads a command string from the connected robot.
 *
 * Blocks until a newline-terminated message is received. Reads one byte
 * at a time to detect the line terminator. Handles both Unix (\n) and
 * Windows (\r\n) line endings.
 *
 * ## Timeout Behavior:
 * - Receive timeout: 240 seconds (4 minutes)
 * - If timeout expires, recv() returns with error
 * - Empty string is returned on timeout or disconnect
 *
 * @return Command string (without line terminators), or empty string on error
 *
 * @note Single-byte recv() is inefficient but simple and reliable for
 *       short command strings typical in this protocol.
 * @note The long timeout (240s) accommodates slow robot operations.
 */
std::string RobotServer::RecvCommand() {
    // Guard against invalid socket
    if (_socket == INVALID_SOCKET) {
        return "";
    }

    std::string r;
    char c;

    // -----------------------------------------------------------------------
    // Configure receive timeout
    // -----------------------------------------------------------------------
    // 240000 ms = 4 minutes
    // Long timeout to accommodate robot operations that may take time
    DWORD t = 240000;
    setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof(t));

    // -----------------------------------------------------------------------
    // Read characters until newline or error
    // -----------------------------------------------------------------------
    // Single-byte reads are simple but inefficient - acceptable for
    // short command strings (typically < 50 bytes)
    while (recv(_socket, &c, 1, 0) > 0) {
        if (c == '\n') break;  // End of message
        r += c;
    }

    // Handle Windows-style line endings (\r\n)
    // Remove trailing carriage return if present
    if (!r.empty() && r.back() == '\r') {
        r.pop_back();
    }

    return r;
}

/**
 * @brief Sends a response string to the connected robot.
 *
 * Appends a '>' character as the message terminator and sends
 * the complete message to the client.
 *
 * @param[in] msg  Response message to send (terminator added automatically)
 *
 * @return true if message was sent successfully, false on error
 *
 * @note Uses '>' as terminator instead of newline for robot compatibility.
 * @note Logs all outgoing messages to console for debugging.
 *
 * @par Example:
 * @code
 *     SendResponse("OK");          // Sends "OK>"
 *     SendResponse("START 10.00"); // Sends "START 10.00>"
 * @endcode
 */
bool RobotServer::SendResponse(const std::string& msg) {
    // Guard against invalid socket
    if (_socket == INVALID_SOCKET) {
        return false;
    }

    // Append terminator character
    // Note: Uses '>' instead of '\n' for robot protocol compatibility
    std::string d = msg + ">";

    // Log outgoing message for debugging
    std::cout << " -> Sending: " << msg << "\n";

    // Send entire message
    // Returns number of bytes sent, or SOCKET_ERROR (-1) on failure
    return send(_socket, d.c_str(), (int)d.size(), 0) > 0;
}

/**
 * @brief Disconnects the active client connection.
 *
 * Gracefully closes the client socket and resets internal state.
 * Safe to call even if no client is currently connected.
 *
 * @note After disconnect, WaitForConnection() can be called again
 *       to accept a new client.
 * @note Does not call WSACleanup() - that's handled by destructor.
 */
void RobotServer::Disconnect() {
    if (_socket != INVALID_SOCKET) {
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
}