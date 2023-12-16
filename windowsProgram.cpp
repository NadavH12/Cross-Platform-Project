#include <algorithm>
#include <iostream>
#include <string>

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <chrono>

#include <ctime>
// Use to convert bytes to KB
#define DIV 1024

// Specify the width of the field in which to print the numbers. 
// The asterisk in the format specifier "%*I64d" takes an integer 
// argument and uses it to pad and right justify the number.
#define WIDTH 7

#ifdef _WIN32
#include <limits.h>
#include <intrin.h>
typedef unsigned __int32  uint32_t;
#else
#include <stdint.h>
#endif

using namespace std;

#define MAX_INTEL_TOP_LVL 4

class CPUID {
    uint32_t regs[4];

    public:
    explicit CPUID(unsigned funcId, unsigned subFuncId) {
#ifdef _WIN32
        __cpuidex((int *)regs, (int)funcId, (int)subFuncId);

#else
        asm volatile
            ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
             : "a" (funcId), "c" (subFuncId));
        // ECX is set to zero for CPUID function 4
#endif
    }

    const uint32_t &EAX() const {return regs[0];}
    const uint32_t &EBX() const {return regs[1];}
    const uint32_t &ECX() const {return regs[2];}
    const uint32_t &EDX() const {return regs[3];}
};

class CPUInfo {
    public:
        CPUInfo();
        string  vendor()            const { return mVendorId;   }
        string  model()             const { return mModelName;  }
        int     cores()             const { return mNumCores;   }
        float   cpuSpeedInMHz()     const { return mCPUMHz;     }
        bool    isSSE()             const { return mIsSSE;      }
        bool    isSSE2()            const { return mIsSSE2;     }
        bool    isSSE3()            const { return mIsSSE3;     }
        bool    isSSE41()           const { return mIsSSE41;    }
        bool    isSSE42()           const { return mIsSSE42;    }
        bool    isAVX()             const { return mIsAVX;      }
        bool    isAVX2()            const { return mIsAVX2;     }
        bool    isHyperThreaded()   const { return mIsHTT;      }
        int     logicalCpus()       const { return mNumLogCpus; }
        string mModelName;


    private:
        // Bit positions for data extractions
        static const uint32_t SSE_POS   = 0x02000000;
        static const uint32_t SSE2_POS  = 0x04000000;
        static const uint32_t SSE3_POS  = 0x00000001;
        static const uint32_t SSE41_POS = 0x00080000;
        static const uint32_t SSE42_POS = 0x00100000;
        static const uint32_t AVX_POS   = 0x10000000;
        static const uint32_t AVX2_POS  = 0x00000020;
        static const uint32_t LVL_NUM   = 0x000000FF;
        static const uint32_t LVL_TYPE  = 0x0000FF00;
        static const uint32_t LVL_CORES = 0x0000FFFF;

        // Attributes
        string mVendorId;
        int    mNumSMT;
        int    mNumCores;
        int    mNumLogCpus;
        float  mCPUMHz;
        bool   mIsHTT;
        bool   mIsSSE;
        bool   mIsSSE2;
        bool   mIsSSE3;
        bool   mIsSSE41;
        bool   mIsSSE42;
        bool   mIsAVX;
        bool   mIsAVX2;
};

CPUInfo::CPUInfo()
{
    // Get vendor name EAX=0
    CPUID cpuID0(0, 0);
    uint32_t HFS = cpuID0.EAX();
    mVendorId += string((const char *)&cpuID0.EBX(), 4);
    mVendorId += string((const char *)&cpuID0.EDX(), 4);
    mVendorId += string((const char *)&cpuID0.ECX(), 4);
    // Get SSE instructions availability
    CPUID cpuID1(1, 0);
    mIsHTT   = cpuID1.EDX() & AVX_POS;
    mIsSSE   = cpuID1.EDX() & SSE_POS;
    mIsSSE2  = cpuID1.EDX() & SSE2_POS;
    mIsSSE3  = cpuID1.ECX() & SSE3_POS;
    mIsSSE41 = cpuID1.ECX() & SSE41_POS;
    mIsSSE42 = cpuID1.ECX() & SSE41_POS;
    mIsAVX   = cpuID1.ECX() & AVX_POS;
    // Get AVX2 instructions availability
    CPUID cpuID7(7, 0);
    mIsAVX2  = cpuID7.EBX() & AVX2_POS;

    string upVId = mVendorId;
    for_each(upVId.begin(), upVId.end(), [](char& in) { in = ::toupper(in); });
    // Get num of cores
    if (upVId.find("INTEL") != std::string::npos) {
        if(HFS >= 11) {
            for (int lvl=0; lvl<MAX_INTEL_TOP_LVL; ++lvl) {
                    CPUID cpuID4(0x0B, lvl);
                    uint32_t currLevel = (LVL_TYPE & cpuID4.ECX())>>8;
                    switch(currLevel) {
                        case 0x01: mNumSMT     = LVL_CORES & cpuID4.EBX(); break;
                        case 0x02: mNumLogCpus = LVL_CORES & cpuID4.EBX(); break;
                        default: break;
                    }
            }
            mNumCores = mNumLogCpus/mNumSMT;
        } else {
            if (HFS>=1) {
                mNumLogCpus = (cpuID1.EBX() >> 16) & 0xFF;
                if (HFS>=4) {
                    mNumCores = 1 + (CPUID(4, 0).EAX() >> 26) & 0x3F;
                }
            }
            if (mIsHTT) {
                if (!(mNumCores>1)) {
                    mNumCores = 1;
                    mNumLogCpus = (mNumLogCpus >= 2 ? mNumLogCpus : 2);
                }
            } else {
                mNumCores = mNumLogCpus = 1;
            }
        }
    } else if (upVId.find("AMD") != std::string::npos) {
        if (HFS>=1) {
            mNumLogCpus = (cpuID1.EBX() >> 16) & 0xFF;
            if (CPUID(0x80000000, 0).EAX() >=8) {
                mNumCores = 1 + (CPUID(0x80000008, 0).ECX() & 0xFF);
            }
        }
        if (mIsHTT) {
            if (!(mNumCores>1)) {
                mNumCores = 1;
                mNumLogCpus = (mNumLogCpus >= 2 ? mNumLogCpus : 2);
            }
        } else {
            mNumCores = mNumLogCpus = 1;
        }
    } else {
        cout<< "Unexpected vendor id" <<endl;
    }
    // Get processor brand string
    // This seems to be working for both Intel & AMD vendors
    for(int i=0x80000002; i<0x80000005; ++i) {
        CPUID cpuID(i, 0);
        mModelName += string((const char*)&cpuID.EAX(), 4);
        mModelName += string((const char*)&cpuID.EBX(), 4);
        mModelName += string((const char*)&cpuID.ECX(), 4);
        mModelName += string((const char*)&cpuID.EDX(), 4);
    }
}


int windows(){
  MEMORYSTATUSEX statex;

  statex.dwLength = sizeof (statex);

  GlobalMemoryStatusEx (&statex);

  _tprintf (TEXT("There is  %*ld percent of memory in use.\n"), //2-
            WIDTH, statex.dwMemoryLoad);
  _tprintf (TEXT("There are %*I64d total KB of RAM.\n"),
            WIDTH, statex.ullTotalPhys/DIV);
  _tprintf (TEXT("There are %*I64d free  KB of RAM.\n"),
            WIDTH, statex.ullAvailPhys/DIV);
  //_tprintf (TEXT("There are %*I64d total KB of paging file.\n"),
    //        WIDTH, statex.ullTotalPageFile/DIV);
  //_tprintf (TEXT("There are %*I64d free  KB of paging file.\n"),
    //        WIDTH, statex.ullAvailPageFile/DIV);
  //_tprintf (TEXT("There are %*I64d total KB of virtual memory.\n"),
    //        WIDTH, statex.ullTotalVirtual/DIV);
  //_tprintf (TEXT("There are %*I64d free  KB of virtual memory.\n"),
    //        WIDTH, statex.ullAvailVirtual/DIV);

  // Show the amount of extended memory available.

  //_tprintf (TEXT("There are %*I64d free  KB of extended memory.\n"),
    //        WIDTH, statex.ullAvailExtendedVirtual/DIV);

  return 0;
}




void getCPUClockSpeed(CPUInfo cinfo){

    string a = cinfo.mModelName;
    string brandString = cinfo.model();
    int lastSpace = brandString.find_last_of(' ');
    string speedString = brandString.substr(lastSpace+1,4);
    double speed = stod(speedString);
    speed = speed * 1000;
    cout << "CPU Clock Speed: " << speed << "Mhz" << endl;
}

void timeSinceEpoch(){

std::time_t result = std::time(nullptr);
std::cout << std::asctime(std::localtime(&result)) << result << " seconds since the Epoch\n";

}




int main(int argc, char *argv[])
{
    CPUInfo cinfo;

    //cout << "CPU vendor = " << cinfo.vendor() << endl;
    cout << "CPU Brand String = " << cinfo.model() << endl;
    
    cout << "# of cores = " << cinfo.cores() << endl;
    cout << "# of logical cores = " << cinfo.logicalCpus() << endl;
    //cout << "Is CPU Hyper threaded = " << cinfo.isHyperThreaded() << endl;
    //cout << "CPU SSE = " << cinfo.isSSE() << endl;
    //cout << "CPU SSE2 = " << cinfo.isSSE2() << endl;
    //cout << "CPU SSE3 = " << cinfo.isSSE3() << endl;
    //cout << "CPU SSE41 = " << cinfo.isSSE41() << endl;
    //cout << "CPU SSE42 = " << cinfo.isSSE42() << endl;
    //cout << "CPU AVX = " << cinfo.isAVX() << endl;
    //cout << "CPU AVX2 = " << cinfo.isAVX2() << endl;

    windows();

   
    getCPUClockSpeed(cinfo);
    timeSinceEpoch();

    return 0;
}