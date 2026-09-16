#pragma once
#include <map>
#include <vector>
#include <cstring>
inline std::map<std::string,std::vector<uint8_t>> stored;
inline bool storageFail=false;
struct Preferences {
 bool begin(const char*,bool){return true;}
 bool isKey(const char* k){return stored.count(k);}
 size_t getBytesLength(const char* k){return stored[k].size();}
 size_t getBytes(const char* k,void* out,size_t n){n=std::min(n,stored[k].size());memcpy(out,stored[k].data(),n);return n;}
 size_t putBytes(const char* k,const void* in,size_t n){if(storageFail)return 0;auto p=(const uint8_t*)in;stored[k]={p,p+n};return n;}
 uint32_t getUInt(const char* k,uint32_t d){uint32_t v=d;if(stored[k].size()==4)memcpy(&v,stored[k].data(),4);return v;}
};
