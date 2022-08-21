#ifndef _MOTOR_H_
#define _MOTOR_H_

#include "Arduino.h"

#define HELICAL_PITCH   8   // 导程8mm
#define MOTOR_STEP		18  // 1.8°

#define UP_LIMIT		70  // 最高行程 80mm

class Motor{
	public:
	
	Motor(int8_t EN, int8_t PUL, int8_t DIR, int8_t LIMIT1, int8_t LIMIT2);
	
	void setSpeed(uint mm_s);
	void setPos(int mm);
	void reset();
	void init();
	void stop();
	
	private:
	int8_t en;
	int8_t pul;
	int8_t dir;
	int8_t limit_down;
	int8_t limit_up;
	
	int current_pos;  // unit: mm
	int speed; // pulse delay, unit:ms
	
	bool init_flag = false;
	bool stop_flag = false;
	
};


#endif