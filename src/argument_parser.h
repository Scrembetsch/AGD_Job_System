#pragma once

#include <string>
#include <algorithm>

class ArgumentParser
{
public:
	int argc;
	char** argv;

	ArgumentParser(int argc = 0, char** argv = nullptr)
		: argc(argc)
		, argv(argv)
	{
	}

	// Gets array position of argument
	int GetPosition(const std::string& shortArg = "", const std::string& longArg = "")
	{
		for (int i = 0; i < argc; i++)
		{
			std::string current(argv[i]);
			if ((shortArg != "" && current == shortArg)
				|| (current != "" && current == longArg))
			{
				return i;
			}
		}
		return -1;
	}

	// Checks if argument exists
	bool CheckIfExists(const std::string& shortArg = "", const std::string& longArg = "")
	{
		return GetPosition(shortArg, longArg) != -1;
	}

	// Retrieves string after argument
	std::string GetString(const std::string& shortArg = "", const std::string& longArg = "", const std::string& defaultValue = "")
	{
		int pos = GetPosition(shortArg, longArg);
		if (pos == -1 || pos == argc)
		{
			return defaultValue;
		}
		return std::string(argv[pos + 1]);
	}

	// Retrieves int after argument
	int GetInt(const std::string& shortArg = "", const std::string& longArg = "", int defaultValue = -1, int base = 10)
	{
		int pos = GetPosition(shortArg, longArg);
		if (pos == -1 || pos == argc)
		{
			return defaultValue;
		}
		return std::stoi(std::string(argv[pos + 1]), nullptr, base);
	}

	// Retrieves float after argument
	float GetFloat(const std::string& shortArg = "", const std::string& longArg = "", float defaultValue = -1.0f)
	{
		int pos = GetPosition(shortArg, longArg);
		if (pos == -1 || pos == argc)
		{
			return defaultValue;
		}
		return std::stof(std::string(argv[pos + 1]));
	}

	// Retrieves bool value (true & 1 -> true; everything else -> false)
	float GetBool(const std::string& shortArg = "", const std::string& longArg = "", bool defaultValue = false)
	{
		int pos = GetPosition(shortArg, longArg);
		if (pos == -1 || pos == argc)
		{
			return defaultValue;
		}
		std::string value(argv[pos + 1]);
		std::transform(value.begin(), value.end(), value.begin(), ::tolower);
		return value == "true" || value == "1";
	}
};
