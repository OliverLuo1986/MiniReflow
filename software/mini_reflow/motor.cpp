#include "motor.h"


Motor::Motor(int8_t EN, int8_t PUL, int8_t DIR, int8_t LIMIT1, int8_t LIMIT2){
	en = EN;
	pul = PUL;
	dir = DIR;
	
	limit_down = LIMIT1;
	limit_up = LIMIT2;
	
	pinMode(en, OUTPUT);
	pinMode(pul, OUTPUT);
	pinMode(dir, OUTPUT);
	
	pinMode(limit_down, INPUT);
	pinMode(limit_up, INPUT);
	
	speed = 10;
	current_pos = -1;
};

void Motor::init(){
	// 构建函数里面的pinMode没效果?
	pinMode(en, OUTPUT);
	pinMode(pul, OUTPUT);
	pinMode(dir, OUTPUT);
	
	pinMode(limit_down, INPUT);
	pinMode(limit_up, INPUT);
}

void Motor::setSpeed(uint ms){
	
	if(ms < 1)
		ms = 1;
	
	if(ms > 100)
		ms = 100;
	
	speed = ms;
}


void Motor::setPos(int pos){
	int left, step, i;
	
	if(pos > UP_LIMIT)
		pos = UP_LIMIT;
	
	if(pos < 0)
		pos = 0;
	
	if(pos == 0)
	{
		reset();
		return;
	}
	
	left = pos-current_pos;
	step = abs(left*360*10/8/MOTOR_STEP);
	printf("left:%d step:%d\n", left, step);
	if(left != 0){
		digitalWrite(en, HIGH);
		
		if(left > 0)
			digitalWrite(dir, LOW);
		else{
			digitalWrite(dir, HIGH);
		}
		
		for(i=0;i<step;i++){
			
			if(left<0){
				if(digitalRead(limit_down)== LOW)
					break;
			}
			
			digitalWrite(pul, HIGH);
			delay(speed);
			digitalWrite(pul, LOW);
			delay(speed);
		}
		
		current_pos = pos;
		
		digitalWrite(en, LOW);
	}
	
	printf("cur pos:%dmm\n", current_pos);
}



void Motor::reset(){
	
	digitalWrite(en, HIGH);
	digitalWrite(dir, HIGH);
	while((digitalRead(limit_down)== HIGH) && !stop_flag ){
		digitalWrite(pul, HIGH);
		delay(speed);
		digitalWrite(pul, LOW);
		delay(speed);
	}
	digitalWrite(en, LOW);
	digitalWrite(dir, LOW);
	
	current_pos = 0;
}