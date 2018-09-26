#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>

#include "bitcoin.hpp"
#include "hash.hpp"
#include "ranger.hpp"
#include "serial.hpp"
#include "threadpool.hpp"

#include "statistics.hpp"

static uint8_t parseHex(uint8_t character);
static uint8_t getHex(uint8_t character);
static void streamHex(std::ostream &out, uint8_t *ptr, size_t len);
static void streamScript(std::ostream &out, uint8_t *ptr, size_t len);

#define MAX_TRANSACTION_SIZE 1024 * 1024 * 30

int main (int argc, char** argv) {
    
    if(argc > 1 && !strcmp(argv[1], "-h")) {
        
        std::cout << argv[0] << " reads a transaction from stdin until EOF is reached, parses, and outputs the result.\n";
        return 0;
    }
    
    std::vector<uint8_t> buffer(MAX_TRANSACTION_SIZE);
    
    size_t bufSize = std::fread(buffer.data(), 1, buffer.size(), stdin);
    
    if(bufSize < 8) {
        
        std::cerr << "Transaction is too small to parse.\n";
        return 1;
    }
    
    if(*(uint32_t*)buffer.data() != 0x01000000 && *(uint32_t*)buffer.data() != 0x02000000) {
        
        if(strncmp((char*)buffer.data(), "01000000", 8) && strncmp((char*)buffer.data(), "02000000", 8)) {
            
            std::cerr << "Transaction does not have needed 'version 1 or 2' flag.\n";
            return 1;
        }
        
        // ASCII hex data has been provided -- let's parse it into binary form.
        
        for(size_t i = 0; i < bufSize / 2; i++)
            buffer[i] = uint8_t(parseHex(buffer[i * 2]) << 4 | parseHex(buffer[i * 2 + 1]));
        
        bufSize /= 2;
    }
    
    auto data = ptr_range(buffer).take(bufSize);
    
    auto transaction = readTransaction(data);
    
    std::cout << "\n";
    
    std::cout << "Transaction version: " << transaction.version << "\n";
    std::cout << "Transaction inputs: " << transaction.inputs.size() << "\n";
    std::cout << "Transaction outputs: " << transaction.outputs.size() << "\n";
    std::cout << "Transaction locktime: " << transaction.locktime << "\n";
    
    std::cout << "\n";
    
    size_t counter = 0;
    
    for(auto input : transaction.inputs) {
        
        counter++;
        
        std::cout << "[Input " << counter << "]\n";
        
        std::cout << "\tHash: ";
        streamHex(std::cout, input.hash.data(), input.hash.size());
        std::cout << "\n";
        
        std::cout << "\tV-out: " << input.vout << "\n";
        
        std::cout << "\tScript: ";
        streamScript(std::cout, input.script.data(), input.script.size());
        std::cout << "\n";
        
        std::cout << "\tSequence: " << input.sequence << "\n";
        
        if(transaction.witnesses.size() > counter - 1) {
            
            for(auto data : transaction.witnesses[counter - 1].stack) {
                
                std::cout << "\tWitness item: ";
                streamHex(std::cout, data.data(), data.size());
                std::cout << "\n";
            }
        }
    }
    
    std::cout << "\n";
    
    counter = 0;
    
    for(auto output : transaction.outputs) {
        
        std::cout << "[Output " << ++counter << "]\n";
        
        std::cout << "\tScript: ";
        streamScript(std::cout, output.script.data(), output.script.size());
        std::cout << "\n";
        
        std::cout << "\tValue: " << double(output.value) / 100000000 << "\n";
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
    
    return 0;
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

static void streamScript(std::ostream &out, uint8_t *ptr, size_t len)
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
                
                out << "Script encountered invalid OP_PUSHDATA -- aborting parse.";
                return;
            }
            
            if(dataSize + i > len) {
                
                out << "Script encountered push data larger than script -- aborting parse.";
                return;
            }
            
            out << "PUSH [";
            streamHex(out, ptr + i, dataSize);
            out << "] ";
            
            i += dataSize - 1;
        }
        else {
            
            out << getOpString(ptr[i]) << " ";
        }
    }
}
