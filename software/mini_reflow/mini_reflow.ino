#include <lvgl.h>
#include <TFT_eSPI.h>
#include <FT6336U.h>
#include <Ticker.h>
#include "reflow.h"
#include "Heater.h"
#include "Motor.h"
#include "max6675.h"


#define RST_N_PIN 4
#define INT_N_PIN 15

#define HEAT_PWM 14
#define FAN_PWM 12

#define EN_PIN      2
#define DIR_PIN     19
#define PUL_PIN     23
#define LIMIT_DOWN  37
#define LIMIT_UP  38

#define MOSI    26
#define CLK     33

#define MAX6675_CS0   32
#define MAX6675_CS1   2
#define MAX6675_CS2   25

#define CHART_LINE_POINT_NUM    (260)

struct Button {
    const uint8_t PIN;
    uint32_t numberKeyPresses;
    bool pressed;
};

Button button1 = {15, 0, false};


FT6336U ft6336u(RST_N_PIN, INT_N_PIN);

Heater heater;
Reflow reflow;
Ticker heaterTick;
Ticker tempTick;
Ticker motorTick;
float temp;
float temp_top;
float temp_bottom;

Motor motor(EN_PIN,PUL_PIN,DIR_PIN,LIMIT_DOWN,LIMIT_UP);

MAX6675 thermocouple1(CLK, MAX6675_CS0, MOSI);
MAX6675 thermocouple2(CLK, MAX6675_CS1, MOSI);
MAX6675 thermocouple3(CLK, MAX6675_CS2, MOSI);

bool motor_running = false;
bool heat_start = false;
bool fan_start = false;
int motor_pos = 0;
bool reflow_start = false;


lv_obj_t* start_btn;


/*Change to your screen resolution*/
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * 10 ];

lv_obj_t* heat_temp_label;
lv_obj_t* top_temp_label;
lv_obj_t* bottom_temp_label;

Ticker chartTick;
lv_obj_t* chart;
lv_chart_series_t* ser_top;
lv_chart_series_t* ser_bottom;

lv_obj_t* tabview;
lv_obj_t* tab_ctrl;
lv_obj_t* tab_chart;
lv_obj_t* tab_setting;

lv_obj_t * heat_icon;

extern const lv_img_dsc_t heat_pic;

static float last_temp_top, last_temp_bottom;

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif

void IRAM_ATTR isr(void* arg) {
    Button* s = static_cast<Button*>(arg);
    s->numberKeyPresses += 1;
    s->pressed = true;
}

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
    tft.endWrite();

    lv_disp_flush_ready( disp );
}



/*Initialize your touchpad*/
static void touchpad_init(void)
{
    Serial.printf("touch init...");
    /*Your code comes here*/
    ft6336u.begin();
}



/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void)
{
    if (button1.pressed) {
        //Serial.printf("Button 1 has been pressed %u times\n", button1.numberKeyPresses);
        button1.pressed = false;
        return true;
    }  
    return false;
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(uint16_t * x, uint16_t * y)
{
    /*Your code comes here*/

    (*x) = ft6336u.read_touch1_x_ext();
    (*y) = ft6336u.read_touch1_y_ext();
    //Serial.printf("touch x:%d  y:%d\n", *x, *y);
}

/*Read the touchpad*/
void my_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
    uint16_t touchX, touchY;

    if(touchpad_is_pressed() == false)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        touchpad_get_xy(&touchX, &touchY);
        /*Set the coordinates*/
        data->point.x = touchX;
        data->point.y = touchY;
#if 0
        Serial.print( "Data x " );
        Serial.println( touchX );

        Serial.print( "Data y " );
        Serial.println( touchY );
#endif        
    }
}


void temp_task(){
  char str[32];
  static long lastMillis = millis();
  long ms;
  
  ms = millis();
  if((ms - lastMillis) >= 500 )
  {
    temp = thermocouple1.readCelsius();
    temp_top = thermocouple3.readCelsius();
    temp_bottom = thermocouple2.readCelsius();

    lastMillis = millis();    

    if(abs(temp_top - last_temp_top)<20)
    {
      last_temp_top = temp_top;
    }
    else
    {
      temp_top = last_temp_top;
    }

    
    if(abs(temp_bottom - last_temp_bottom)<20)
    {
      last_temp_bottom = temp_bottom;
    }
    else
    {
      temp_bottom = last_temp_bottom;
    }
    
    sprintf(str, "%dC", (int)temp);
    lv_label_set_text(heat_temp_label, str);

    sprintf(str, "%dC", (int)temp_top);
    lv_label_set_text(top_temp_label, str); 

    sprintf(str, "%dC", (int)temp_bottom);
    lv_label_set_text(bottom_temp_label, str);

    if(heat_start)
    {
      if(temp_bottom>170)
      {
        ledcWrite(1, 25*1024/100);
      }
      else if(temp_bottom < 155)
      {
        ledcWrite(1, 0);
      }
    }
    
    //printf("C1:%3.2f  C2:%3.2f  C3:%3.2f\n", temp, temp_top, temp_bottom); 
  }
}


float getTemp(void){
  return temp_top;
}

void heater_Handle(){
  heater.handle();
}


void heater_Init(){
  heater.setTargetTemp(80.0);
  heater.setGetTempCallback(getTemp);
  heater.setPWMpin(HEAT_PWM, FAN_PWM);
  heater.setKp(20);
  heater.setKi(0.01);  
  heater.setKd(200);
  heater.init(); 

  heaterTick.attach_ms(400, heater_Handle);
}

void reflow_state_change_callback(void *param, E_REFLOW_STATE state)
{
  if(state == REFLOW_STATE4_COOLING)
  {   
      motor.setSpeed(6);
      motor_pos = 0;
      motorTick.attach_ms(1, motor_task);

      lv_obj_t* label = lv_obj_get_child(start_btn, 0);
      heat_start = false;
      lv_label_set_text(label, "Heat Start");    

      heat_icon_hide();
  }
  else if(state == REFLOW_STATE5_END)
  {
    
  }
}

void reflow_init()
{
  reflow.init(&heater);
  reflow.set_soak_temp(150);
  reflow.set_soak_time(60);
  reflow.set_reflow_temp(210);
  reflow.set_reflow_time(30);
  reflow.set_state_change_callback(reflow_state_change_callback, &reflow);
}


void motor_task()
{
  motor.setPos(motor_pos);

  motorTick.detach();
}


static void slider_event_callback(lv_event_t * e)
{
   int value;
   char str[32];
   
   lv_obj_t * slider = lv_event_get_target(e);
   lv_obj_t* label = (lv_obj_t* )lv_event_get_user_data(e);
   
   value = (int)lv_slider_get_value(slider);
   sprintf(str, "%d mm", value);
   lv_label_set_text(label, str);
   
    motor_pos =value;
    motorTick.attach_ms(1, motor_task);
}



void motor_slider_init()
{
    lv_obj_t * slider = lv_slider_create(tab_ctrl); // 创建滑块对象
    if (slider != NULL)
    {
        lv_obj_set_width(slider, 140); // 设置slider的宽度
        lv_obj_set_pos(slider, 140, 25);
        lv_slider_set_range(slider, 0, 80); // 设置滑块值的变化范围0-80

        lv_obj_t* pos_label = lv_label_create(tab_ctrl); // 创建一个标签，用于显示滑块的滑动值
        if (pos_label != NULL)
        {
            lv_label_set_text(pos_label, "Pos:"); // 标签默认显示滑块的最小值
            lv_obj_align_to(pos_label, slider, LV_ALIGN_OUT_TOP_MID, -30, -10);
        }         
 
        lv_obj_t* label = lv_label_create(tab_ctrl); // 创建一个标签，用于显示滑块的滑动值
        if (label != NULL)
        {
            lv_label_set_text_fmt(label, "%d mm", lv_slider_get_min_value(slider)); // 标签默认显示滑块的最小值
            lv_obj_align_to(label, slider, LV_ALIGN_OUT_TOP_MID, 10, -10);  // 标签对象放在slider对象的上方中间位置
  
            lv_obj_add_event_cb(slider, slider_event_callback, LV_EVENT_VALUE_CHANGED, (void *)label);
        }
    }
}


static void slider_event_callback1(lv_event_t * e)
{
   int value;
   char str[32];
   
   lv_obj_t * slider = lv_event_get_target(e);
   lv_obj_t* label = (lv_obj_t* )lv_event_get_user_data(e);
   
   value = (int)lv_slider_get_value(slider);
   sprintf(str, "%d%%", value);
   lv_label_set_text(label, str);

   ledcWrite(1, value*1023/100);
}



void fan_slider_init()
{
    lv_obj_t * slider = lv_slider_create(tab_ctrl); // 创建滑块对象
    if (slider != NULL)
    {
        lv_obj_set_width(slider, 140); // 设置slider的宽度
        lv_obj_set_pos(slider, 140, 80);
        lv_slider_set_range(slider, 0, 100); // 设置滑块值的变化范围0-70

        lv_obj_t* fan_label = lv_label_create(tab_ctrl); // 创建一个标签，用于显示滑块的滑动值
        if (fan_label != NULL)
        {
            lv_label_set_text(fan_label, "Fan:"); // 标签默认显示滑块的最小值
            lv_obj_align_to(fan_label, slider, LV_ALIGN_OUT_TOP_MID, -25, -10);
        }        
 
        lv_obj_t* label = lv_label_create(tab_ctrl); // 创建一个标签，用于显示滑块的滑动值
        if (label != NULL)
        {
            lv_label_set_text_fmt(label, "%d %%", lv_slider_get_min_value(slider)); // 标签默认显示滑块的最小值
            lv_obj_align_to(label, slider, LV_ALIGN_OUT_TOP_MID, 10, -10);
  
            // 添加滑块值变化事件和事件回调函数，并将label对象最为事件的user_data
            lv_obj_add_event_cb(slider, slider_event_callback1, LV_EVENT_VALUE_CHANGED, (void *)label);
        }
    }
}


static void btn_event_callback(lv_event_t* event)
{
    static uint32_t counter = 1;
 
    lv_obj_t* btn = lv_event_get_target(event); //获取事件对象
    if (btn != NULL)
    {
        lv_obj_t* label = lv_obj_get_child(btn, 0); // 获取第一个子对象
        if (label != NULL)
        {
            if(heat_start){
              heat_start = false;
              lv_label_set_text(label, "Heat Start");
              heat_icon_hide();
             
              reflow.stop();
              
              motor_pos = 0;
              motorTick.attach_ms(1, motor_task);
              
              ledcWrite(1,1023); 
            }
            else{
              heat_start = true;
              lv_label_set_text(label, "Heat Stop");

              heat_icon_show();
              
              ledcWrite(1,0);
              reflow.start();

              motor_pos =75;
              motorTick.attach_ms(1, motor_task);
            } 
        }
    }
}
 
void start_button_init()
{
    start_btn = lv_btn_create(tab_ctrl); // 创建Button对象
    if (start_btn != NULL)
    {
        lv_obj_set_size(start_btn, 120, 50); // 设置对象大小,宽度和高度
        lv_obj_set_pos(start_btn, 140, 130); // 设置按钮位置，即X和Y坐标
        lv_obj_add_event_cb(start_btn, btn_event_callback, LV_EVENT_CLICKED, NULL); // 给对象添加CLICK事件和事件处理回调函数
 
        lv_obj_t* label = lv_label_create(start_btn); // 基于Button对象创建Label对象
        if (label != NULL)
        {
            lv_label_set_text(label, "Heat Start"); // 设置显示内容
            lv_obj_center(label); // 对象居中显示
        }
    }
}

void label_init(){

  top_temp_label = lv_label_create(tab_ctrl);
  lv_obj_set_pos(top_temp_label, 52, 80);
  lv_label_set_text(top_temp_label, "");

  bottom_temp_label = lv_label_create(tab_ctrl);
  lv_obj_set_pos(bottom_temp_label, 52, 115);
  lv_label_set_text(bottom_temp_label, "");  
}

void heat_pic_init()
{
    static lv_style_t style_btn; 
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_palette_lighten(LV_PALETTE_RED, 1));
    
    start_btn = lv_btn_create(tab_ctrl); // 创建Button对象
    if (start_btn != NULL)
    {
        lv_obj_add_style(start_btn, &style_btn, LV_STATE_DEFAULT); 
        
        lv_obj_set_size(start_btn, 100, 24); // 设置对象大小,宽度和高度
        lv_obj_set_pos(start_btn, 10, 10); // 设置按钮位置，即X和Y坐标

        heat_temp_label = lv_label_create(start_btn); // 基于Button对象创建Label对象
        if (heat_temp_label != NULL)
        {
            lv_label_set_text(heat_temp_label, "25"); // 设置显示内容
            lv_obj_center(heat_temp_label); // 对象居中显示
        }
    }




  
}

void heat_icon_show()
{
    heat_icon = lv_img_create(tab_ctrl);
    lv_img_set_src(heat_icon,&heat_pic);
    lv_obj_set_pos(heat_icon, 35, 36);
}

void heat_icon_hide()
{
    lv_obj_del(heat_icon);
}

void pcb_pic_init()
{
    static lv_style_t style_btn; 
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_palette_lighten(LV_PALETTE_GREEN, 1));
    
    start_btn = lv_btn_create(tab_ctrl); // 创建Button对象
    if (start_btn != NULL)
    {
        lv_obj_add_style(start_btn, &style_btn, LV_STATE_DEFAULT); 
        
        lv_obj_set_size(start_btn, 100, 12); // 设置对象大小,宽度和高度
        lv_obj_set_pos(start_btn, 10, 100); // 设置按钮位置，即X和Y坐标

        lv_obj_t* label = lv_label_create(start_btn); // 基于Button对象创建Label对象
        if (label != NULL)
        {
            lv_label_set_text(label, "PCB"); // 设置显示内容
            lv_obj_center(label); // 对象居中显示
        }
    }
}

void chart_draw()
{
  static int i = 0;
  static int j = 0;
  static long lastMillis = millis();
  long ms;
  
  ms = millis();
  if((ms - lastMillis) >= 2000 )
  {  
    lastMillis = millis();
   
    if(i < CHART_LINE_POINT_NUM)
    {
      ser_top->y_points[i++] = (int)temp_top;
      ser_bottom->y_points[j++] = (int)temp_bottom;
       
    }
    else
    {
      int k = 0;
      for(k=0;k<CHART_LINE_POINT_NUM-1;k++)
      {
        ser_top->y_points[k] = ser_top->y_points[k+1];
      } 
      ser_top->y_points[CHART_LINE_POINT_NUM-1] = (int)temp_top;
  
      for(k=0;k<CHART_LINE_POINT_NUM-1;k++)
      {
        ser_bottom->y_points[k] = ser_bottom->y_points[k+1];
      } 
      ser_bottom->y_points[CHART_LINE_POINT_NUM-1] = (int)temp_bottom;
    }
  
    lv_chart_refresh(chart); 
  }

  //printf("C1:%d  C2:%d  C3:%d\n", (int)temp, (int)temp_top, (int)temp_bottom);
}

void chart_init()
{
  chart = lv_chart_create(tab_chart);
  lv_obj_set_size(chart, 270, 180);
  lv_obj_set_pos(chart, 20, 0);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);

  lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,0,250);
  lv_chart_set_point_count(chart, CHART_LINE_POINT_NUM);

  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2,3 ,2, true, 40);
  
  ser_top = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
  ser_bottom = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
}

static void tabview_init(void)
{
    tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 30);

    tab_ctrl = lv_tabview_add_tab(tabview, "Control");
    tab_chart = lv_tabview_add_tab(tabview, "Chart");
    tab_setting = lv_tabview_add_tab(tabview, "Setting");

    static lv_style_t main_style; 
    lv_style_init(&main_style);
    lv_style_set_bg_color(&main_style, lv_color_make(0xff, 0xff, 0xff));  
    lv_obj_add_style(tab_ctrl, &main_style, LV_STATE_DEFAULT);  
    lv_obj_add_style(tab_chart, &main_style, LV_STATE_DEFAULT); 
    lv_obj_add_style(tab_setting, &main_style, LV_STATE_DEFAULT);    
}


void setup()
{
    Serial.begin( 115200 ); /* prepare for possible serial debug */

    heater_Init();
    reflow_init();

    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb( my_print ); /* register print function for debugging */
#endif

    tft.begin();          /* TFT init */
    tft.setRotation( 1 ); /* Landscape orientation, flipped */

    lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * 10 );

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

    /*Initialize touchpad */
    touchpad_init();
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register( &indev_drv );

    // touch interrupt init
    ft6336u.setRotation(Rotation_1);
    pinMode(button1.PIN, INPUT_PULLUP);
    attachInterruptArg(button1.PIN, isr, &button1, FALLING);    
    
    tabview_init();
    pcb_pic_init();
    heat_pic_init();
    label_init();

    fan_slider_init();
    motor_slider_init();   
    start_button_init();
    chart_init();

    motor.init();
    motor.setSpeed(2);
    motor_pos = 0;
    motorTick.attach_ms(1, motor_task);

    last_temp_top = thermocouple3.readCelsius();
    last_temp_bottom = thermocouple2.readCelsius();
    
    // setup finish
    Serial.println( "Setup done" );
}

void loop()
{
    lv_task_handler();
    lv_tick_inc(1);
    reflow.task();
    temp_task();
    chart_draw();
    delay(1);
}
