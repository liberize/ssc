#ifndef OBFUSCATOR_H
#define OBFUSCATOR_H

#include <string>

#define OBF(str) obf::c_str(str)
#define OBF_BYTES(...) obf::bytes((const char[]) { __VA_ARGS__ })

namespace obf {

template<size_t index>
struct encryptor {
	static constexpr void encrypt(char *dest, const char *str, short key) {
		dest[index] = str[index] ^ (char) key;
		encryptor<index - 1>::encrypt(dest, str, key - 1);
	}
};

template<>
struct encryptor<0> {
	static constexpr void encrypt(char *dest, const char *str, short key) {
		dest[0] = str[0] ^ (char) key;
	}
};

template<size_t S>
class string {
public:
	constexpr string(const char str[S], short key, bool nullTerminated) :
		buffer_{}, decrypted_{false}, key_{key}, nullTerminated_{nullTerminated} {
		encryptor<S - 1>::encrypt(buffer_, str, key);
	}

	const char *data() {
		if (decrypted_) {
			return buffer_;
		}
		for (int i = S - 1; i >= 0; i--) {
			buffer_[i] ^= (char) (key_ - ((S - 1) - i));
		}
		decrypted_ = true;
		return buffer_;
	}

	size_t size() {
		return nullTerminated_ ? S - 1 : S;
	}

	operator std::string() {
		return std::string(data(), size());
	}

private:
	mutable char buffer_[S];
	mutable bool decrypted_;
	const short key_;
	bool nullTerminated_;
};

template<size_t S>
static constexpr auto c_str(const char(&str)[S]) {
	return string<S>{ str, S, true };
}

template<size_t S>
static constexpr auto bytes(const char(&str)[S]) {
	return string<S>{ str, S, false };
}

}

#endif
