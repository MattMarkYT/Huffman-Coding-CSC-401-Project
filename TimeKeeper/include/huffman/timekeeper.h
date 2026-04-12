//
// Created by timothy on 4/11/26.
//

#ifndef TIME_KEEPER_H
#define TIME_KEEPER_H
#include <string>

using namespace std;

void startTimer();

void stopTimer();

void saveTimer(const string &timerName);

void resetTimer();

void eraseAll();

long timerGetTotalMicro(const string &timerName);

double timerGetTotalMilli(const string &timerName);

double timerGetTotalSeconds(const string &timerName);

#endif //TIME_KEEPER_H