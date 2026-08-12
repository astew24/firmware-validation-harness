#pragma once

#include <atomic>
#include <functional>
#include <string>

// Minimal HTTP server for Prometheus scrapes: serves GET /metrics on
// 127.0.0.1:port until `running` turns false. Returns false if the port
// cannot be opened.
bool serve_metrics(int port, const std::function<std::string()>& render,
                   const std::atomic<bool>& running);
