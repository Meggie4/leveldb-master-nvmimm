/*
 * timer.h
 *
 *  Created on: Sep 24, 2016
 *      Author: pandian
 */

#ifndef UTIL_TIMER_H_
#define UTIL_TIMER_H_

#include <assert.h>
#include "leveldb/env.h"
#include "util/logging.h"

#define TIMER_LOG
//#define TIMER_LOG_SEEK
//#define TIMER_LOG_SIMPLE

#define NUM_SEEK_THREADS 4

namespace leveldb {

enum TimerStep {
	BEGIN,
    /////////////meggie
    WRITE_IMMU_TO_LEVEL0,	
    TOTAL_MOVE_TO_NVMTABLE,
    GET_IMMUTABLE_BATCHES,
    /////////////meggie
	END
};
class Timer {

private:
	uint64_t micros_array[200];
	uint64_t timer_micros[200];
	uint64_t timer_count[200];
	uint64_t timer_count_additional[200];
	std::string message[200];

public:
	Timer() {
		init();
	}

	~Timer() {
	}

	void init() {
		message[WRITE_IMMU_TO_LEVEL0] = "WRITE_IMMU_TO_LEVEL0";
		message[TOTAL_MOVE_TO_NVMTABLE] = "TOTAL_MOVE_TO_NVMTABLE";
        message[GET_IMMUTABLE_BATCHES] = "GET_IMMUTABLE_BATCHES";
		clear();
	}

	void StartTimer(TimerStep step) {
        //获取当前的时间戳
		micros_array[step] = Env::Default()->NowMicros();
	}

	void Record(TimerStep step) {
		assert(micros_array[step] != 0);
		timer_micros[step] += Env::Default()->NowMicros() - micros_array[step];
		timer_count[step]++;
	}

	void Record(TimerStep step, uint64_t additional_count) {
		assert(micros_array[step] != 0);
        //记录该步骤所耗的时间
		timer_micros[step] += Env::Default()->NowMicros() - micros_array[step];
        //记录该步骤所用的次数
        timer_count[step]++;
		timer_count_additional[step] += additional_count;
	}

	void clear() {
        //将成员属性数组全部清0
		for (int i = BEGIN; i < END; i++) {
			micros_array[i] = timer_micros[i] = timer_count[i] = timer_count_additional[i] = 0;
		}
	}

	std::string DebugString() {
		std::string result;
        /////打印出所有步骤总共消耗的时间，总共经历的次数，以及额外数据
        //printf("begin:%d, end:%d\n", BEGIN, END);
		for (int i = BEGIN; i < END; i++) {
            //printf("\nmessage:%d, ", i);
			if (timer_count[i] > 0) {
                //printf("print timer_micros, ");
				result.append(message[i])
						.append(": timer_micros: ");
				AppendNumberTo(&result, timer_micros[i]);
                //printf("print timer_count, ");
				result.append(" timer_count: ");
				AppendNumberTo(&result, timer_count[i]);
                //printf("print timer_count_additional");
				result.append(" timer_count_additional: ");
				AppendNumberTo(&result, timer_count_additional[i]);
				result.append("\n");
			}
            //printf(",end");
		}
        //返回相应的步骤信息
		return result;
	}

	void AppendTimerInfo(Timer* timer) {
        //将多个timer的信息结合起来
		if (timer == NULL) {
			return;
		}
		for (int i = BEGIN; i < END; i++) {
			if (timer->timer_count[i] > 0) {
				timer_count[i] += timer->timer_count[i];
				timer_count_additional[i] += timer->timer_count_additional[i];
				timer_micros[i] += timer->timer_micros[i];
			}
		}
	}
};

}

#endif /* UTIL_TIMER_H_ */
