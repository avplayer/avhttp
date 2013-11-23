#include <boost/assert.hpp>
#include "avhttp.hpp"

int main(int argc, char* argv[])
{
	std::vector<char> unreserved_chars;
	for(char i = 'a'; i <= 'z'; ++i)
	{
		unreserved_chars.push_back(i);
	}
	for(char i = 'A'; i <= 'Z'; ++i)
	{
		unreserved_chars.push_back(i);
	}
	for(char i = '0'; i <= '9'; ++i)
	{
		unreserved_chars.push_back(i);
	}
	unreserved_chars.push_back('-');
	unreserved_chars.push_back('_');
	unreserved_chars.push_back('.');
	unreserved_chars.push_back('~');

	std::set<char> unescaped_chars_set(unreserved_chars.begin(), unreserved_chars.end());

	std::vector<char> all_asci_vector;
	for(size_t i = 0; i <= 0xff; ++i)
	{
		all_asci_vector.push_back(static_cast<char>(i));
	}
	
	std::string str(&all_asci_vector[0], all_asci_vector.size());
	auto encoded = avhttp::detail::escape_string(str);

	assert(encoded.size() == (all_asci_vector.size() - unreserved_chars.size()) * 3 + unreserved_chars.size());

	int position = 0;
	for(size_t i = 0; i < all_asci_vector.size(); ++i)
	{
		char ch = all_asci_vector[i];
		if (unescaped_chars_set.end() != unescaped_chars_set.find(ch))
		{
			assert(encoded[position] == ch);
			position += 1;
		}
		else
		{
			char out_str[3];
			avhttp::detail::to_hex(&ch, 1, out_str);

			assert(encoded[position] == '%');
			assert(encoded[position+1] == out_str[0]);
			assert(encoded[position+2] == out_str[1]);

			position += 3;
		}
	}

	std::string decoded;
	bool res = avhttp::detail::unescape_path(encoded, decoded);
	assert(res);
	assert(decoded == str);

	return 0;
}
