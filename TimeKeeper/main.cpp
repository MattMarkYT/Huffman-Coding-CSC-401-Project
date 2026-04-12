#include <chrono>
#include <iostream>
#include <thread>

#include "huffman/timekeeper.h"

using namespace std;

int main() {
    int passes = 0;
    cout << "Test 1 (Accuracy): ";
    {   // **************** //
        // Test 1: Accuracy //
        // **************** //
        auto start = chrono::steady_clock::now();
        this_thread::sleep_for(std::chrono::milliseconds(100));
        auto end   = chrono::steady_clock::now();

        startTimer();
        this_thread::sleep_for(std::chrono::milliseconds(100));
        stopTimer();
        saveTimer("Accuracy");
        resetTimer();

        // abs(chrono - TimeKeeper <= 30 microseconds)
        if (abs(duration_cast<chrono::microseconds>(end - start).count()
            - timerGetTotalMicro("Accuracy")) <= 30) {
            passes++;
            cout << "PASSED" << endl;
            }
        else {
            cout << "FAILED (" <<
                duration_cast<chrono::microseconds>(end - start).count() <<
                    " vs " << timerGetTotalMicro("Accuracy") << ")" << endl;
        }
    }

    cout << "Test 2 (Precision): ";
    {   // ***************** //
        // Test 2: Precision //
        // ***************** //
        auto start = chrono::steady_clock::now();
        this_thread::sleep_for(std::chrono::microseconds(10));
        auto end   = chrono::steady_clock::now();

        startTimer();
        this_thread::sleep_for(std::chrono::microseconds(10));
        stopTimer();
        saveTimer("Precision");
        resetTimer();

        // abs(chrono - TimeKeeper <= 2 microseconds)
        if (abs(duration_cast<chrono::microseconds>(end - start).count()
            - timerGetTotalMicro("Precision")) <= 2) {
            passes++;
            cout << "PASSED" << endl;
            }
        else {
            cout << "FAILED (" <<
                duration_cast<chrono::microseconds>(end - start).count() <<
                    " vs " << timerGetTotalMicro("Precision") << ")" << endl;
        }
    }

    cout << "Test 3 (Edge Case): ";
    {   // **************************** //
        // Test 3: Edge Case (No Sleep) //
        // **************************** //
        auto start = chrono::steady_clock::now();
        auto end   = chrono::steady_clock::now();

        startTimer();
        stopTimer();
        saveTimer("Edge Case");
        resetTimer();

        // abs(chrono - TimeKeeper <= 1 microseconds)
        if (abs(duration_cast<chrono::microseconds>(end - start).count()
            - timerGetTotalMicro("Edge Case")) <= 1) {
            passes++;
            cout << "PASSED" << endl;
            }
        else {
            cout << "FAILED (" <<
                duration_cast<chrono::microseconds>(end - start).count() <<
                    " vs " << timerGetTotalMicro("Edge Case") << ")" << endl;
        }
    }

    cout << "Test 4 (Unit Conversions): ";
    {   // ************************ //
        // Test 4: Unit Conversions //
        // ************************ //
        auto start = chrono::steady_clock::now();
        this_thread::sleep_for(std::chrono::seconds(1));
        auto end   = chrono::steady_clock::now();

        startTimer();
        this_thread::sleep_for(std::chrono::seconds(1));
        stopTimer();
        saveTimer("Unit Conversions");
        resetTimer();

        const long chrono_us = duration_cast<chrono::microseconds>(end - start).count();
        const double chrono_ms = static_cast<double>(chrono_us) / 1000.0;
        const double chrono_s = static_cast<double>(chrono_us) / 1000000.0;

        // abs(chrono_us - TimeKeeper_us <= 100 microseconds)
        // && abs(chrono_ms - TimeKeeper_ms <= 100 microseconds)
        // && abs(chrono_s - TimeKeeper_s <= 100 microseconds)
        if (abs(chrono_us - timerGetTotalMicro("Unit Conversions")) <= 100
            && abs(chrono_ms - timerGetTotalMilli("Unit Conversions")) <= 0.1
            && abs(chrono_s - timerGetTotalSeconds("Unit Conversions")) <= 0.0001) {
            passes++;
            cout << "PASSED" << endl;
            }
        else {
            cout << "FAILED (" <<
                chrono_us << " vs " << timerGetTotalMicro("Unit Conversions") << " || " <<
                chrono_ms << " vs " << timerGetTotalMilli("Unit Conversions") << " || " <<
                chrono_s << " vs " << timerGetTotalSeconds("Unit Conversions") << ")" << endl;
        }
    }

    cout << "All Tests: " << (passes == 4 ? "PASSED" : "FAILED") << endl;

    return 0;
}