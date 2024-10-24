#include <string_view>
#include <tuple>

namespace Utility
{
	/// @brief Checks if a string matches the pattern [-+]?[0-9]+*\.?[0-9]+ without regex
	/// @param s
	/// @return True if the string is a number, false at the first character found that makes s ill-formed as a number
	bool IsNumber(const std::string_view &s);

	/// @brief Checks if a string is composed only of digits
	/// @param s
	/// @return True if no non-digit characters found, false at the first non-digit character
	bool IsDigitsOnly(const std::string_view &s);

	size_t GetFirstNonNumericPosition(const std::string_view &s);
	size_t FindFirstUnescaped(const std::string_view &s, const char c);
	size_t GetDelimitedBlockLength(const std::string_view &s, const char Delimiter);
	std::string GetDelimitedBlock(const std::string_view &s, const char Delimiter);
	const std::string_view GetStringViewSubrange(const std::string_view &s, size_t Start, size_t End);
	void RemoveNonPrintableCharacters(std::string &s);
	std::string RemoveNonPrintableCharacters(const std::string_view &sv);

	/// @brief Base class for processing blocks of a string. Intends to replicate a small subset of istringstream functionality while only creating primitives.
	class BlockStringProcessor
	{
	private:
		size_t ProcessedBlocks{0};
		size_t ProcessedCharacters{0};

	protected:
		/// @brief How the derived class processes a block
		/// @return std::tuple with: *bool*: True if the block is usable. *size_t*: Start position of the block. *size_t*: End position of the block. *size_t*: Total characters processed.
		virtual std::tuple<bool, size_t, size_t, size_t> ProcessBlock() = 0;
		const std::string_view Block;

	public:
		BlockStringProcessor(const std::string_view &s) : Block{s} {}
		BlockStringProcessor(const std::string_view &s, size_t Start, size_t End) : BlockStringProcessor(s.substr(Start, End - Start)) {}
		virtual ~BlockStringProcessor() = default;
		const std::string_view GetNextBlock();
		size_t GetProcessedBlocks() const { return ProcessedBlocks; }
		size_t GetProcessedCharacters() const { return ProcessedCharacters; }
		bool More() const { return ProcessedCharacters < Block.size(); }
	};

	class DelimitedBlockProcessor : public BlockStringProcessor
	{
	protected:
		char Delimiter;
		virtual std::tuple<bool, size_t, size_t, size_t> ProcessBlock() override;

	public:
		DelimitedBlockProcessor(const std::string_view &s, char Delimiter) : BlockStringProcessor(s), Delimiter{Delimiter} {}
		DelimitedBlockProcessor(const std::string_view &s, size_t Start, size_t End, char Delimiter) : BlockStringProcessor(s, Start, End), Delimiter{Delimiter} {}
		virtual ~DelimitedBlockProcessor() = default;
	};
}
