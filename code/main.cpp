#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>

// Platform-specific includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib") // Link with Winsock library
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#define SERVER_PORT 8080

const std::string HTTP_RESPONSE =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 13\r\n"
"Connection: close\r\n\r\n"
"Hello, World!";

std::mutex console_mutex; // Mutex for thread-safe console output

// Cross-platform socket close function
#ifdef _WIN32
#define CLOSESOCKET closesocket
#else
#define CLOSESOCKET close
#endif

void handle_client(int client_socket) {
  try {
    char buffer[1024] = { 0 };

    // Read the HTTP request
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
      std::cerr << "Error reading from client.\n";
      CLOSESOCKET(client_socket);
      return;
    }

    // Print the request to the console (thread-safe)
    {
      std::lock_guard<std::mutex> lock(console_mutex);
      std::cout << "Received request:\n" << buffer << std::endl;
    }

    // Send the HTTP response
    send(client_socket, HTTP_RESPONSE.c_str(), HTTP_RESPONSE.size(), 0);

    // Close the connection
    CLOSESOCKET(client_socket);
  }
  catch (const std::exception& e) {
    std::cerr << "Error handling client: " << e.what() << std::endl;
  }
}

int main()
{
  const int port = SERVER_PORT;

  // Platform-specific socket initialization
#ifdef _WIN32
  WSADATA wsa_data;
  int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (wsa_result != 0) {
    std::cerr << "WSAStartup failed: " << wsa_result << std::endl;
    return 1;
  }
#endif

  // Create a socket
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    std::cerr << "Socket creation failed.\n";
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  // Bind the socket to an address and port
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
    std::cerr << "Bind failed.\n";
    CLOSESOCKET(server_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  // Listen for incoming connections
  if (listen(server_socket, 10) < 0) {
    std::cerr << "Listen failed.\n";
    CLOSESOCKET(server_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  std::cout << "HTTP server is running on port " << port << "...\n";

  std::vector<std::thread> threads;

  while (true) {
    // Accept an incoming connection
    int client_socket = accept(server_socket, nullptr, nullptr);
    if (client_socket < 0) {
      std::cerr << "Accept failed.\n";
      continue;
    }

    // Launch a thread to handle the client
    threads.emplace_back(std::thread(handle_client, client_socket));
  }

  // Close the server socket (this won't be reached here)
  CLOSESOCKET(server_socket);

  // Clean up threads (optional if the server runs indefinitely)
  for (auto& thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

#ifdef _WIN32
  WSACleanup();
#endif

  return 0;
}