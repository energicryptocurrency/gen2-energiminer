/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file main.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 * Ethereum client.
 */

#include "MinerAux.h"

#include <thread>
#include <fstream>
#include <iostream>

using namespace std;

namespace energi
{
  extern int g_logVerbosity;
}

// Currently tested for little endian systems
int is_little_endian() { return 0x1 == *reinterpret_cast<const uint16_t*>("\1\0"); }

int main(int argc, char** argv)
{
    if ( !is_little_endian() ) {
        cerr << "Big endian not tested" << endl;
        exit(-1);
    }

    try {
        // Set env vars controlling GPU driver behavior.
        energi::setenv("GPU_MAX_HEAP_SIZE", "100");
        energi::setenv("GPU_MAX_ALLOC_PERCENT", "100");
        energi::setenv("GPU_SINGLE_ALLOC_PERCENT", "100");

        if (getenv("SYSLOG"))
            g_logSyslog = true;
        if (g_logSyslog || (getenv("NO_COLOR")))
            g_logNoColor = true;
#if defined(_WIN32)
        if (!g_logNoColor) {
            g_logNoColor = true;
            // Set output mode to handle virtual terminal sequences
            // Only works on Windows 10, but most users should use it anyway
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE) {
                DWORD dwMode = 0;
                if (GetConsoleMode(hOut, &dwMode)) {
                    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    if (SetConsoleMode(hOut, dwMode))
                        g_logNoColor = false;
                }
            }
        }
#endif

        MinerCLI m;
        m.ParseCommandLine(argc, argv);



        m.execute();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return -1;
    }
    return 0;
}
