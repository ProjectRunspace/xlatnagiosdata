#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <tuple>
#include "utility.hpp"

class NumberRunner
{
protected:
	virtual bool ShouldContinue(const size_t Position, char Character, const bool IsNumeric) = 0;
	void RunString(const std::string_view &s)
	{
		bool DecimalFound{false};
		bool IsNumeric;
		for (size_t Position{0}; Position < s.size(); ++Position)
		{
			IsNumeric = false;
			switch (s[Position])
			{
			case '+':
				[[fallthrough]];
			case '-':
				if (Position == 0)
				{
					IsNumeric = true;
				}
				break;
			case '.':
				if (!DecimalFound)
				{
					DecimalFound = true;
					IsNumeric = true;
				}
				break;
			default:
				IsNumeric = std::isdigit(s[Position]);
				break;
			}
			if (!ShouldContinue(Position, s[Position], IsNumeric))
			{
				return;
			}
		}
	}

public:
	NumberRunner() = default;
	virtual ~NumberRunner() = default;
};

class NumberValidator : public NumberRunner
{
private:
	bool IsNumber{false};

protected:
	bool ShouldContinue(const size_t, char, const bool IsNumeric) override
	{
		IsNumber = IsNumeric;
		return IsNumeric;
	}

public:
	NumberValidator() = default;
	virtual ~NumberValidator() = default;
	bool IsValidNumber(const std::string_view &s)
	{
		RunString(s);
		return IsNumber;
	}
};

class FirstNonNumeric : public NumberRunner
{
private:
	size_t NonNumericPosition{0};
	bool FoundNonNumeric{false};

protected:
	bool ShouldContinue(const size_t Position, char, const bool IsNumeric) override
	{
		if (!IsNumeric)
		{
			NonNumericPosition = Position;
			FoundNonNumeric = true;
		}
		return IsNumeric;
	}

public:
	FirstNonNumeric() = default;
	virtual ~FirstNonNumeric() = default;

	size_t GetFirstPosition(const std::string_view &s)
	{
		RunString(s);
		if (FoundNonNumeric)
		{
			return NonNumericPosition;
		}
		else
		{
			return s.size();
		}
	}
};

bool Utility::IsNumber(const std::string_view &s)
{
	return NumberValidator{}.IsValidNumber(s);
}

// simpler to use built-in functions than the custom NumberRunner class
bool Utility::IsDigitsOnly(const std::string_view &s)
{
	return std::all_of(s.begin(), s.end(), [](unsigned char c)
							 { return std::isdigit(c); });
}

size_t Utility::GetFirstNonNumericPosition(const std::string_view &s)
{
	return FirstNonNumeric{}.GetFirstPosition(s);
}

size_t Utility::FindFirstUnescaped(const std::string_view &s, const char c)
{
	for (size_t i{0}; i < s.size(); i++)
	{
		if (s[i] == c && (i == 0 || (s[i - 1] != '\\')))
		{
			return i;
		}
	}
	return std::string::npos;
}

std::string Utility::GetDelimitedBlock(const std::string_view &s, const char Delimiter)
{
	auto End{FindFirstUnescaped(s, Delimiter)};
	if (End == std::string::npos)
	{
		return std::string{s};
	}
	return std::string{s.substr(0, End)};
}

size_t Utility::GetDelimitedBlockLength(const std::string_view &s, const char Delimiter)
{
	auto End{FindFirstUnescaped(s, Delimiter)};
	if (End == std::string::npos)
	{
		return s.size() - 1;
	}
	return End;
}

const std::string_view Utility::GetStringViewSubrange(const std::string_view &s, size_t Start, size_t End)
{
	if (s.empty())
	{
		return s;
	}
	if (End >= s.size())
	{
		End = s.size() - 1;
	}
	if (Start > End)
	{
		Start = End;
	}
	return s.substr(Start, End + 1 - Start);
}

void Utility::RemoveNonPrintableCharacters(std::string &s)
{
	s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c)
								  { return !std::isprint(c); }),
			  s.end());
}

std::string Utility::RemoveNonPrintableCharacters(const std::string_view &sv)
{
	std::string s{sv};
	RemoveNonPrintableCharacters(s);
	return s;
}

const std::string_view Utility::BlockStringProcessor::GetNextBlock()
{
	if (More())
	{
		auto [EmptyBlock, StartPosition, EndPosition, BlockProcessedCharCount]{ProcessBlock()};
		if (EmptyBlock)
		{
			return std::string_view{};
		}
		if (EndPosition >= Block.size() - 1)
		{
			EndPosition = Block.size() - 1;
		}
		ProcessedBlocks++;
		ProcessedCharacters += BlockProcessedCharCount;
		return GetStringViewSubrange(Block, StartPosition, EndPosition);
	}
	return std::string_view{};
}

std::tuple<bool, size_t, size_t, size_t> Utility::DelimitedBlockProcessor::ProcessBlock()
{
	bool Empty{false};
	auto Start{GetProcessedCharacters()};
	size_t BlockProcessedCharCount{1}; // will always process at least 1 character, if for no other reason than to advance the position
	auto End{FindFirstUnescaped(Block.substr(Start), Delimiter)};
	if (End == Start)
	{
		Empty = true;
	}
	else if (End != std::string::npos)
	{
		--End += Start;
		BlockProcessedCharCount++; // otherwise we'd fail to account for the delimiter
	}
	else
	{
		End = Block.size();
	}
	BlockProcessedCharCount += End - Start;
	return std::make_tuple(Empty, Start, End, BlockProcessedCharCount);
}