//
// Created by timothy on 4/11/26.
//

#include "../../include/huffman/timekeeper.h"
#include <chrono>
#include <iostream>
#include <unordered_map>

using namespace std::chrono;

auto start_time = high_resolution_clock::time_point{};
long total_time = 0;
std::unordered_map<string, long> total_times;

void startTimer() {
    start_time = high_resolution_clock::now();
}

void stopTimer() {
    const auto end_time = high_resolution_clock::now();
    if (start_time == high_resolution_clock::time_point{}) {
        cout << "Timer needs to be started first!" << endl;
        return;
    }
    total_time += duration_cast<microseconds>(end_time - start_time).count();
    start_time = high_resolution_clock::time_point{};
}

void saveTimer(const string &timerName) {
    total_times[timerName] = total_time;
}

void resetTimer() {
    total_time = 0;
}

void eraseAll() {
    total_times.clear();
    start_time = high_resolution_clock::time_point{};
    total_time = 0;
}

long timerGetTotalMicro(const string &timerName) {
    if (!total_times.contains(timerName)) {
        return -1.0;
    }
    return total_times[timerName];
}

double timerGetTotalMilli(const string &timerName) {
    if (!total_times.contains(timerName)) {
        return -1.0;
    }
    return static_cast<double>(total_times[timerName]) / 1000.0;
}

double timerGetTotalSeconds(const string &timerName) {
    if (!total_times.contains(timerName)) {
        return -1.0;
    }
    return static_cast<double>(total_times[timerName]) / 1000000.0;
}