#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>

#include <cstdint>
#include <cstring>

#include <secp256k1.h>

#include <openssl/sha.h>
#include <openssl/ripemd.h>

#include <curl/curl.h>

#include <unistd.h>
#include <fcntl.h>

std::string generate_address(uint8_t privkey[32]);

int main()
{
    std::string json;

    unsigned iteration = 0;
    while (true)
    {
        std::cout << "\033[33m" << "Iteration " << iteration++ << "\033[0m" << " - ";

        uint8_t privkey[32];

        std::string url = "https://blockchain.info/balance?active=";

        int fd = open("/dev/random", O_RDONLY);
        for (int i = 0; i < 100 - 1; ++i)
        {
            read(fd, privkey, sizeof(privkey));
            url += "|" + generate_address(privkey);
        }
        close(fd);

        CURL* curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t size, size_t nmemb, std::string* data) {
            data->append(ptr, size*nmemb);
            return size*nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        size_t position = -1;
        while ((position = json.find("final_balance", position + 1)) != std::string::npos)
        {
            auto temp = json.substr(position, 16);
            if (temp.back() != '0')
            {   
                std::cout << "\033[1;32m" << "SUCCESS" << "\033[0m" << std::endl;
                break;
            }
        }

        std::cout << "\033[1;31m" << "FAIL" << "\033[0m" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::ofstream dump("dump.json");
    dump << json;

    return 0;
}


bool base58(char *b58, size_t *b58sz, const uint8_t *data, size_t binsz)
{
    static const char b58digits_ordered[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

	const uint8_t *bin = data;
	int carry;
	ssize_t i, j, high, zcount = 0;
	uint8_t buf[12 * 1024] = {0};
	size_t size;

	// ???????????????????? ???????????????????? ?????????????????? ?????????? ???????????? ?????? ?????????????????????? 
	while (zcount < (ssize_t)binsz && !bin[zcount])
		++zcount;

	// ???????????????????? ???????????? ??????????????, ???????????????????????? ?????? ???????????????? ?????????????????????????????? ???????????? 138 / 100-> log (256) / log (58)
	size = (binsz - zcount) * 138 / 100 + 1;
	memset(buf, 0, size);
	
	// ?????????????? ???????????? ?????? ????????????????????????????
	for (i = zcount, high = size - 1; i < (ssize_t)binsz; ++i, high = j)
	{
		// ?????????????????? ???????????? ?????????????????????????????? ???? ???????????? ???? ??????????
		for (carry = bin[i], j = size - 1; (j > high) || carry; --j)
		{
			carry += 256 * buf[j];
			buf[j] = carry % 58;
			carry /= 58;
		}
	}

	for (j = 0; j < (ssize_t)size && !buf[j]; ++j);

	if (*b58sz <= zcount + size - j)
	{
		*b58sz = zcount + size - j + 1;
		return false;
	}

	if (zcount)
		memset(b58, '1', zcount);
	for (i = zcount; j < (ssize_t)size; ++i, ++j)
		b58[i] = b58digits_ordered[buf[j]];
	b58[i] = '\0';
	*b58sz = i + 1;

	return true;
}

std::string generate_address(uint8_t privkey[32])
{

// Stage 1 /////////////////////////////////////////////////////////////////

    uint8_t stage1[65];

    //////////

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);

    if (!secp256k1_ec_seckey_verify(ctx, privkey))
    {
        std::cerr << "Invalid private key\n";
        return {};
    }
    
    secp256k1_pubkey pubkey;
    auto _ = secp256k1_ec_pubkey_create(ctx, &pubkey, privkey);
    size_t outlen = 65;
    secp256k1_ec_pubkey_serialize(ctx, stage1, &outlen, &pubkey, SECP256K1_EC_UNCOMPRESSED);

    secp256k1_context_destroy(ctx);

// Stage 2 /////////////////////////////////////////////////////////////////

    uint8_t stage2[32];

    //////////

    SHA256(stage1, sizeof(stage1), stage2);

// Stage 3 /////////////////////////////////////////////////////////////////

    uint8_t stage3[20];

    //////////

    RIPEMD160(stage2, sizeof(stage2), stage3);

// Stage 4 /////////////////////////////////////////////////////////////////

    uint8_t stage4[21];

    //////////

    stage4[0] = 0x00;
    for (size_t i = 0; i < sizeof(stage3); ++i) stage4[i + 1] = stage3[i];

// Stage 5 /////////////////////////////////////////////////////////////////

    uint8_t stage5[32];

    //////////

    SHA256(stage4, sizeof(stage4), stage5);

// Stage 6 /////////////////////////////////////////////////////////////////

    uint8_t stage6[32];

    //////////

    SHA256(stage5, sizeof(stage5), stage6);

// Stage 7 /////////////////////////////////////////////////////////////////

    uint8_t stage7[4];

    //////////

    for (size_t i = 0; i < sizeof(stage7); ++i) stage7[i] = stage6[i];

// Stage 8 /////////////////////////////////////////////////////////////////

    uint8_t stage8[25];

    //////////

    for (size_t i = 0; i < sizeof(stage4); ++i) stage8[i] = stage4[i];
    for (size_t i = 0; i < sizeof(stage7); ++i) stage8[i + sizeof(stage4)] = stage7[i];

// Stage 9 /////////////////////////////////////////////////////////////////

    std::string stage9;

    //////////

    char buffer[64] = {};
    size_t size = sizeof(buffer);
    base58(buffer, &size, stage8, sizeof(stage8));

    stage9.assign(buffer);

///////////////////////////////////////////////////////////////////////////

    return stage9;
}
