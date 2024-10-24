#pragma once

#include "config.hpp"
#include "logwriter.hpp"

class N2IDaemon
{
private:
	Configuration Config{};
	std::unique_ptr<ILogWriter> Log{nullptr};

	void LoadConfiguration();

public:
	N2IDaemon() = default;
	~N2IDaemon() = default;
	N2IDaemon(const N2IDaemon &) = delete;
	N2IDaemon &operator=(const N2IDaemon &) = delete;
	N2IDaemon(N2IDaemon &&) = default;
	N2IDaemon &operator=(N2IDaemon &&) = default;

	void Run();
};