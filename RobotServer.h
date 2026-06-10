/**
 * @file RobotServer.h
 * @brief TCP/IP server for robot controller communication.
 *
 * This class implements a simple TCP server that listens for connections from
 * a robot controller and exchanges text-based commands. It provides the
 * communication bridge between the measurement system and external automation.
 *
 * ## Communication Protocol:
 * ```
 * Robot Controller                Scanner Application
 *        │                               │
 *        │◄────── (listening) ───────────┤ WaitForConnection()
 *        │                               │
 *        ├─── "READY\n" ────────────────►│ RecvCommand()
 *        │◄── "START 10.00\n" ───────────┤ SendResponse()
 *        │                               │
 *        ├─── "SCAN_START\n" ───────────►│
 *        │◄── "OK\n" ────────────────────┤
 *        │                               │
 *        │   (robot moves, scanner captures)
 *        │                               │
 *        ├─── "SCAN_END\n" ─────────────►│
 *        │◄── "OK\n" ────────────────────┤
 *        │                               │
 *        ├─── "QUIT\n" ─────────────────►│
 *        │         (disconnect)          │
 * ```
 *
 * ## Supported Commands:
 * | Command     | Description                        | Expected Response       |
 * |-------------|------------------------------------|-------------------------|
 * | READY       | Robot asks what to do next         | START <step> / BUSY / DONE |
 * | SCAN_START  | Begin profile capture              | OK / BUSY / DONE        |
 * | SCAN_END    | End capture, trigger analysis      | OK / ERR                |
 * | QUIT        | Disconnect gracefully              | (none)                  |
 *
 * ## Message Format:
 * - All messages are newline-terminated ASCII strings
 * - Maximum message length: 256 bytes
 * - No binary data or special characters
 *
 * ## Typical Usage:
 * @code
 *     RobotServer server;
 *     if (server.WaitForConnection(5010)) {
 *         while (true) {
 *             std::string cmd = server.RecvCommand();
 *             if (cmd.empty()) break;  // Disconnected
 *
 *             if (cmd == "READY") {
 *                 server.SendResponse("START 10.00");
 *             }
 *             // ... handle other commands
 *         }
 *     }
 * @endcode
 *
 * @version 1.0
 * @date 2026-02-05
 *
 * @note Requires Winsock2 library (ws2_32.lib)
 * @note Only supports one client connection at a time
 *
 * @see RobotControlLoop() in GetProfiles_Callback_Measure.cpp for usage
 */

#pragma once

#include <string>
#include <winsock2.h>   // Windows Sockets 2 API
#include <ws2tcpip.h>   // TCP/IP specific functions (inet_pton, etc.)
#include <iostream>

 /** @brief Links the Winsock2 library automatically */
#pragma comment(lib, "ws2_32.lib")

/**
 * @class RobotServer
 * @brief Single-client TCP server for robot command/response communication.
 *
 * Manages a TCP socket that listens for incoming connections and provides
 * simple text-based message exchange. Designed for integration with industrial
 * robot controllers that support TCP/IP communication.
 *
 * ## Lifecycle:
 * 1. Constructor initializes Winsock
 * 2. WaitForConnection() blocks until client connects
 * 3. RecvCommand() / SendResponse() exchange messages
 * 4. Disconnect() or destructor cleans up
 *
 * ## Thread Safety:
 * Not thread-safe. All methods should be called from the same thread.
 *
 * @note Only one client can be connected at a time.
 * @note Blocking I/O is used - calls will wait until data is available.
 */
class RobotServer {
private:
    /**
     * @brief Connected client socket handle.
     *
     * Set to INVALID_SOCKET when no client is connected.
     * Assigned by WaitForConnection() when a client connects.
     */
    SOCKET _socket = INVALID_SOCKET;

    /**
     * @brief Winsock initialization status.
     *
     * True if WSAStartup() succeeded in constructor.
     * If false, all socket operations will fail.
     */
    bool _initialized = false;

public:
    /**
     * @brief Constructs the server and initializes Winsock.
     *
     * Calls WSAStartup() to initialize the Windows Sockets library.
     * If initialization fails, _initialized is set to false and
     * subsequent operations will fail gracefully.
     *
     * @note Does not start listening - call WaitForConnection() separately.
     */
    RobotServer();

    /**
     * @brief Destructor - cleans up socket and Winsock resources.
     *
     * Disconnects any active client and calls WSACleanup() to
     * release Winsock resources.
     */
    ~RobotServer();

    /**
     * @brief Blocks until a robot controller connects on the specified port.
     *
     * Creates a listening socket, binds to the specified port, and waits
     * for an incoming connection. This is a blocking call that will not
     * return until a client connects or an error occurs.
     *
     * @param[in] port  TCP port number to listen on (e.g., 5010)
     *
     * @return true if a client connected successfully, false on error
     *
     * @note Only one client can connect at a time.
     * @note The listening socket is closed after accepting a connection.
     * @note Typical port: 5010 (configurable based on robot setup)
     *
     * @par Example:
     * @code
     *     RobotServer server;
     *     std::cout << "Waiting for robot on port 5010...\n";
     *     if (server.WaitForConnection(5010)) {
     *         std::cout << "Robot connected!\n";
     *     }
     * @endcode
     */
    bool WaitForConnection(unsigned short port);

    /**
     * @brief Reads a command string from the connected robot.
     *
     * Blocks until a newline-terminated message is received from the client.
     * The newline character is stripped from the returned string.
     *
     * @return Command string (without newline), or empty string on disconnect/error
     *
     * @note Blocking call - will wait indefinitely for data.
     * @note Maximum message length: 256 bytes
     * @note Empty return value indicates connection was lost.
     *
     * @par Example:
     * @code
     *     std::string cmd = server.RecvCommand();
     *     if (cmd.empty()) {
     *         std::cout << "Robot disconnected\n";
     *     } else {
     *         std::cout << "Received: " << cmd << "\n";
     *     }
     * @endcode
     */
    std::string RecvCommand();

    /**
     * @brief Sends a response string to the connected robot.
     *
     * Appends a newline character and sends the message to the client.
     *
     * @param[in] msg  Response message to send (newline added automatically)
     *
     * @return true if message was sent successfully, false on error
     *
     * @note Newline is automatically appended - do not include in msg.
     *
     * @par Example:
     * @code
     *     server.SendResponse("OK");        // Sends "OK\n"
     *     server.SendResponse("START 10.00"); // Sends "START 10.00\n"
     * @endcode
     */
    bool SendResponse(const std::string& msg);

    /**
     * @brief Disconnects the active client connection.
     *
     * Closes the client socket and resets internal state.
     * Safe to call even if no client is connected.
     *
     * @note Called automatically by destructor.
     * @note After disconnect, WaitForConnection() can be called again.
     */
    void Disconnect();
};