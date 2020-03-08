#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <openssl/sha.h>
#include <openssl/ripemd.h>

#include "bitcoin.hpp"
#include "hash.hpp"
#include "ranger.hpp"
#include "serial.hpp"
#include "threadpool.hpp"

#include "statistics.hpp"

// returns 0xff on failure
static uint8_t parseHex(uint8_t character);

static uint8_t getHex(uint8_t character);

static void streamHex(std::ostream &out, uint8_t *ptr, size_t len);

static bool streamScript(std::ostream &out, uint8_t *ptr, size_t len);

static std::vector<uint8_t> doSha256(std::vector<uint8_t> data);

static std::vector<uint8_t> doRipemd160(std::vector<uint8_t> data);

#define MAX_TRANSACTION_SIZE 1024 * 1024 * 30

int main (int argc, char** argv) {

    if(argc > 1 && !strcmp(argv[1], "-h")) {

        std::cout << argv[0] << " reads a transaction from stdin until EOF is reached, parses, and outputs the result.\n"
        "The transaction can be in binary or ASCII hex form.\n"
        "EOF can typically be supplied on the command line with Control D.\n";

        return 0;
    }

    std::vector<uint8_t> buffer(MAX_TRANSACTION_SIZE);

    size_t bufSize = std::fread(buffer.data(), 1, buffer.size(), stdin);

    if(bufSize < 8) {

        std::cerr << "Transaction is too small to parse.\n";
        return 1;
    }

    if(parseHex(buffer[0]) != 0xff && parseHex(buffer[1]) != 0xff) {

        // ASCII hex data has been provided -- let's parse it into binary form.

        for(size_t i = 0; i < bufSize / 2; i++)
            buffer[i] = uint8_t(parseHex(buffer[i * 2]) << 4 | parseHex(buffer[i * 2 + 1]));

        bufSize /= 2;
    }

    if(*(uint32_t*)buffer.data() != 0x00000001 && *(uint32_t*)buffer.data() != 0x00000002) {

        std::cerr << "Transaction does not have needed 'version 1 or 2' flag.\n";
        std::cerr << "We will attempt to read it as if it were a script and then abort.\n";

        streamScript(std::cout, buffer.data(), bufSize);
        std::cout << "\n";

        return 1;
    }

    auto transaction = readTransaction(ptr_range(buffer).take(bufSize));

    std::cout << "\n";
    std::cout << "Transaction version: " << transaction.version << ", locktime: " << transaction.locktime << "\n";
    std::cout << "\n";

    size_t counter = 0;

    for(auto input : transaction.inputs) {

        counter++;

        std::cout << "[Input " << counter << "]\n";

        std::cout << "\tHash: ";
        streamHex(std::cout, input.hash.data(), input.hash.size());
        std::cout << "\n";

        std::cout << "\tV-out: " << input.vout << "\n";

        bool outputSuccess = false;

        if(input.scriptStack.size() == 1) {

            std::vector<uint8_t> script(input.scriptStack[0].begin(), input.scriptStack[0].end());

            std::cout << "\tHASH160(Script): [";
            streamHex(std::cout, doRipemd160(doSha256(script)).data(), 20);
            std::cout << "]\n";

            std::cout << "\tScript input: [";
            streamHex(std::cout, script.data(), script.size());
            std::cout << "]\n";

            outputSuccess = true;
        }
        else if(input.scriptStack.size() > 1) {

            std::vector<uint8_t> script(input.scriptStack.back().begin(), input.scriptStack.back().end());

            // Is this [probably] a public key?
            if(script.size() && script[0] > 2 && script[0] < 6) {

                std::cout << "\tHASH160(Public Key): [";
                streamHex(std::cout, doRipemd160(doSha256(script)).data(), 20);
                std::cout << "]\n";

                std::cout << "\tPublic Key: ";
                streamHex(std::cout, script.data(), script.size());
                std::cout << "\n";

                if(input.scriptStack.size() > 1) {

                    std::cout << "\tSignatures:\n";

                    for(size_t i = 0; i < input.scriptStack.size() - 1; i++) {

                        std::cout << "\t[";
                        streamHex(std::cout, input.scriptStack[i].begin(), input.scriptStack[i].size());
                        std::cout << "]\n";
                    }
                }

                outputSuccess = true;
            }
            else { // Otherwise, it's [probably] a script

                std::cout << "\tHASH160(Script): [";
                streamHex(std::cout, doRipemd160(doSha256(script)).data(), 20);
                std::cout << "]\n";

                std::cout << "\tScript: ";
                outputSuccess = streamScript(std::cout, script.data(), script.size());
                std::cout << "\n";

                if(outputSuccess && input.scriptStack.size() > 1) {

                    std::cout << "\tScript inputs:\n";

                    for(size_t i = 0; i < input.scriptStack.size() - 1; i++) {

                        std::cout << "\t[";
                        streamHex(std::cout, input.scriptStack[i].begin(), input.scriptStack[i].size());
                        std::cout << "]\n";
                    }
                }
            }
        }

        if(!outputSuccess && input.script.size()) {

            std::cout << "\tRaw script: ";
            streamScript(std::cout, input.script.begin(), input.script.size());
            std::cout << "\n";
        }

        std::cout << "\tSequence: " << input.sequence << " aka. 0x";
        streamHex(std::cout, (uint8_t*)&input.sequence, sizeof(input.sequence));
        std::cout << "\n";

        std::cout << "\tWitness flag: " << getOpString(input.witnessFlag) << "\n";

        if(counter - 1 < transaction.witnesses.size()) {

            auto stack = transaction.witnesses[counter - 1].stack;

            if(input.witnessFlag == OP_P2WPKH) {

                if(stack.size() != 2) {

                    std::cerr << "\tOP_P2WPKH script has incorrect number of witness elements!\n";
                }
                else {

                    std::vector<uint8_t> pubkey(stack[1].begin(), stack[1].end());

                    std::cout << "\tHASH160(Witness pubkey): [";
                    streamHex(std::cout, doRipemd160(doSha256(pubkey)).data(), 20);
                    std::cout << "]\n";

                    std::cout << "\tWitness pubkey: [";
                    streamHex(std::cout, pubkey.data(), pubkey.size());
                    std::cout << "]\n";

                    std::cout << "\tWitness signature: [";
                    streamHex(std::cout, stack[0].begin(), stack[0].size());
                    std::cout << "]\n";
                }
            }
            else if(input.witnessFlag == OP_P2WSH) {

                if(stack.size() == 0) {

                    std::cerr << "\tOP_P2WSH script has no witness elements!\n";
                }
                else {

                    std::vector<uint8_t> script(stack.back().begin(), stack.back().end());

                    std::cout << "\tSHA256(Witness script): [";
                    streamHex(std::cout, doSha256(script).data(), 20);
                    std::cout << "]\n";

                    std::cout << "\tWitness script: ";
                    streamScript(std::cout, script.data(), script.size());
                    std::cout << "\n";

                    size_t i = 0;

                    std::cout << "\tWitness script inputs: ";

                    for(; i < stack.size() - 1; i++) {

                        std::cout << (i ? "\t[" : "[");
                        streamHex(std::cout, stack[i].begin(), stack[i].size());
                        std::cout << "]\n";
                    }
                }
            }
            else if(input.witnessFlag != OP_NOWITNESS) {

                std::cout << "\tUnknown witness type. Dumping all witness data:\n";

                for(auto data : stack) {

                    std::cout << "\tWitness item: ";
                    streamHex(std::cout, data.begin(), data.size());
                    std::cout << "\n";
                }
            }
        }
    }

    std::cout << "\n";

    counter = 0;

    for(auto output : transaction.outputs) {

        std::cout << "[Output " << ++counter << "]\n";

        if(!output.script.size()) {

            std::cout << "\tScript: EMPTY SCRIPT\n";
        }
        else {

            std::cout << "\tScript: ";
            streamScript(std::cout, output.script.data(), output.script.size());
            std::cout << "\n";
        }

        std::cout << "\tValue: " << output.value / 100000000 << ".";

        printf("%08llu\n", output.value % 100000000);
    }

    std::cout << "\n";

    return 0;
}

static uint8_t parseHex(uint8_t character)
{
    if(character >= '0' && character <= '9')
        return uint8_t(character - '0');

    if(character >= 'a' && character <= 'f')
        return uint8_t(character - 'a' + 10);

    if(character >= 'A' && character <= 'F')
        return uint8_t(character - 'A' + 10);

    return 0xff;
}

static uint8_t getHex(uint8_t character)
{
    if(character <= 9)
        return uint8_t(character + '0');

    if(character <= 16)
        return uint8_t(character - 10 + 'a');

    return '?';
}

static void streamHex(std::ostream &out, uint8_t *ptr, size_t len)
{
    for(size_t i = 0; i < len; i++)
        out << getHex(uint8_t(ptr[i] >> 4)) << getHex(ptr[i] & 0x0f);
}

static bool streamScript(std::ostream &out, uint8_t *ptr, size_t len)
{
    for(size_t i = 0; i < len; i++) {

        if(ptr[i] > OP_0 && ptr[i] <= OP_PUSHDATA4) {

            uint32_t dataSize = ptr[i];

            i++;

            if(dataSize == OP_PUSHDATA1 && i + 1 < len) {

                dataSize = ptr[i];

                i++;
            }
            else if(dataSize == OP_PUSHDATA2 && i + 2 < len) {

                dataSize = *(uint16_t*)(ptr + i);

                i += 2;
            }
            else if(dataSize == OP_PUSHDATA4 && i + 4 < len) {

                dataSize = *(uint32_t*)(ptr + i);

                i += 4;
            }

            if(dataSize == OP_PUSHDATA1 || dataSize == OP_PUSHDATA2 || dataSize == OP_PUSHDATA4) {

                out << "Script encountered invalid OP_PUSHDATA -- aborting parse. ";
                return false;
            }

            if(dataSize + i > len) {

                out << "Script encountered push data larger than script -- aborting parse. ";
                return false;
            }

            out << "PUSH(" << dataSize << ") [";
            streamHex(out, ptr + i, dataSize);
            out << "] ";

            i += dataSize - 1;
        }
        else {

            out << getOpString(ptr[i]) << " ";
        }
    }

    return true;
}

static std::vector<uint8_t> doSha256(std::vector<uint8_t> data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256_CTX ctx;

    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data.data(), data.size());
    SHA256_Final(hash, &ctx);

    return std::vector<uint8_t>(hash, hash + SHA256_DIGEST_LENGTH);
}

static std::vector<uint8_t> doRipemd160(std::vector<uint8_t> data)
{
    unsigned char hash[RIPEMD160_DIGEST_LENGTH];

    RIPEMD160_CTX ctx;

    RIPEMD160_Init(&ctx);
    RIPEMD160_Update(&ctx, data.data(), data.size());
    RIPEMD160_Final(hash, &ctx);

    return std::vector<uint8_t>(hash, hash + RIPEMD160_DIGEST_LENGTH);
}
