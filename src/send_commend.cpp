//
// Created by zgd on 18-4-20.
//

#include "send_commend.h"
#include <vector>
#include "Serial_RT.h"
#include <iostream>
using namespace std;
//è?·é??è??????????€???é???????§è?????
char switch_hover[5]={'\xFF','\x02','\x04','\xE7','\x05'};//????????°????????????

char unlock_Throttl[5]={'\xFF','\x02','\x02','\x57','\x04'};//è§?é?????é??
char unlock_Yaw[5]={'\xFF','\x02','\x03','\x82','\x07'};//è§?é????????
char recover_Yaw[5]={'\xFF','\x02','\x03','\xD0','\x05'};//è§?é??????????????Yaw????????°???é?????(??????é???????????????°±???è§?é?????è°??????????????????§)
char push_Throttl[5]={'\xFF','\x02','\x02','\xD0','\x06'};//è?·é??????????????è?±??°?????????é??è????????é???€???????é????????,????????????é??
char Throttl[5]={'\xFF','\x02','\x02','\xFD','\x05'};//è?·é????????é???????€,??????è??????????€?????????é???????????è??500ms???é????????é???????°???é??????????????

//é??è???????€
const char stop_forward[5]={'\xFF','\x02','\x01','\xDC','\x05'};//前后回中命令
char go_forward[5]={'\xFF','\x02','\x01','\xDC','\x05'};//向前命令初始化为中间值具体数值在函数中加
char go_back[5]={'\xFF','\x02','\x01','\xDC','\x05'};//向后命令初始化为中间值具体数值在函数中加



const char stop_cross[5]={'\xFF','\x02','\x00','\xDC','\x05'};//左右回中命令
char go_left[5]={'\xFF','\x02','\x00','\xDC','\x05'};//向左初始化为中间值具体数值在函数中加
char go_right[5]={'\xFF','\x02','\x00','\xDC','\x05'};//向右初始化为中间值具体数值在函数中加


char turn_left[5]={'\xFF','\x02','\x03','\xA4','\x05'};//?·?è??
const char stop_rotation[5]={'\xFF','\x02','\x03','\xD0','\x05'};//旋转回中
char turn_right[5]={'\xFF','\x02','\x03','\x13','\x06'};//???è??
char hover[5]={'\xFF','\x02','\x02','\xFD','\x05'};//??????é???????????
char go_up[5]={'\xFF','\x02','\x02','\xD0','\x06'};//??????
char go_down[5]={'\xFF','\x02','\x02','\x4A','\x07'};//???é??
char self_check[5]={'\xFF','\x02','\x04','\x57','\x04'};//自稳模式
//é??è??
char land[5]={'\xFF','\x02','\x04','\xBA','\x06'};//é??è??,??????é??è?????????????????é??è?????é???????????????????????

void take_off(){
    uart_write(command_serial_fd,self_check,5);
	usleep(1000*5000);
   	uart_write(command_serial_fd,switch_hover,5);//????????°????????????
    usleep(1000*100);
	uart_write(command_serial_fd,switch_hover,5);//????????°????????????
    usleep(1000*100);
    uart_write(command_serial_fd,stop_forward,5);
    usleep(1000*100);
    uart_write(command_serial_fd,stop_left,5);	
    usleep(1000*100);
    uart_write(command_serial_fd,unlock_Throttl,5);//è§?é?????é??
    uart_write(command_serial_fd,unlock_Yaw,5);//è§?é????????
    usleep(1000*5000);
    uart_write(command_serial_fd,recover_Yaw,5);//????????????
    usleep(1000*1000);
    uart_write(command_serial_fd,push_Throttl,5);//??????é??
    usleep(1000*2500);
    uart_write(command_serial_fd,Throttl,5);
}

void send_stop_front(){
	uart_write(command_serial_fd,stop_forward,5);
}


void send_go_forward(){
    uart_write(command_serial_fd,go_forward,5);
	sleep(15);
	uart_write(command_serial_fd,stop_forward,5);
}

void send_go_back(){
	uart_write(command_serial_fd,go_back,5);
	sleep(15);
	uart_write(command_serial_fd,stop_back,5);

}
//è§???????????????°
void theta_hold(double theta) {

     uart_write(command_serial_fd,stop_rotation,5);
//	cout<<"theta="<<theta<<endl;
    double deviation=(theta-90);//è§????????·????90???????????????è§?????????????????·?è??è§????????€§??????è??è§????????°?
    
    if(abs(deviation)>=30){
	cout<<deviation<<endl;
	uart_write(command_serial_fd,land,5);
	}

    else if(deviation>0) {
        uart_write(command_serial_fd, turn_left, 5);

	usleep(1000*50);
     uart_write(command_serial_fd,stop_rotation,5);
//	cout<<"theta="<<theta<<endl;
    }
    else if(deviation<0){
        uart_write(command_serial_fd,turn_right,5);

     usleep(1000*50);
     uart_write(command_serial_fd,stop_rotation,5);
//	cout<<"theta="<<theta<<endl;
    }

}

//???é??é??è???????€
void send_land(){
    uart_write(command_serial_fd,land,5);
    sleep(60);
    uart_write(command_serial_fd,switch_hover,5);//é??è?????????????????é??????????????????????????

}
void send_go_left(){
	uart_write(command_serial_fd,go_left,5);
}
void send_go_right(){

	uart_write(command_serial_fd,go_right,5);
}

void send_stop_cross(){
	uart_write(command_serial_fd,stop_left,5);
}


/*int generate_command(int dst_x,int dst_y,int cur_X,int cur_Y){
    //?????€???????????°è??????????
	cout<<"dst_x="<<dst_x<<"dst_y"<<dst_y<<endl;
    if(abs(cur_X-dst_x)<=30&&abs(cur_Y-dst_y<=30)){
        uart_write(command_serial_fd,stop_forward,5);
        uart_write(command_serial_fd,stop_back,5);
        uart_write(command_serial_fd,stop_left,5);
        uart_write(command_serial_fd,stop_right,5);
        //???é??è?????é???????????è°?????????€
        sleep(3);
        return 1;//è???????°è??
    }
    else{
	//cout<<"dst_x="<<dst_x<<endl;
        if((cur_X-dst_x)<0){
            //?????????????????€
		
            uart_write(command_serial_fd,go_forward,5);
	//	cout<<"forward success"<<endl;
        }
        else{
            //?????????????????€
            uart_write(command_serial_fd,go_back,5);
        }
        if((cur_Y-dst_y)<0){
            //??????????????€
            uart_write(command_serial_fd,go_right,5);
        }
        else{
            //???????·??????€
            uart_write(command_serial_fd,go_left,5);
        }
    }
    return 0;
}*/

int generate_command(int dst_x,int dst_y,int str_x,int str_y,int cur_X,int cur_Y){
    //先判断是否到达目的?
	//cout<<"dst_x="<<dst_x<<"dst_y"<<dst_y<<endl;

    if(dst_x==str_x){//y方向飞行
        if(abs(cur_Y-dst_y)<=30){
            return 1;
        }
        else{
            if (cur_Y-dst_y>0){
                uart_write(command_serial_fd,go_left,5);
                
            }
            else if(cur_Y-dst_y<0){
                uart_write(command_serial_fd,go_right,5);
            }
        }
    }
    else if(dst_y==str_y){//x方向飞行
        if(abs(cur_X-dst_x)<=30){
            return 1;
        }
        else{
            if (cur_X-dst_x>0){
                uart_write(command_serial_fd,go_back,5);

            }
            else if(cur_X-dst_x<0){
                uart_write(command_serial_fd,go_forward,5);
            }
        }
    }
return 0;
}

void send_hover() {
	    uart_write(command_serial_fd,stop_forward,5);	
	    uart_write(command_serial_fd,stop_back,5);
	    uart_write(command_serial_fd,stop_left,5);
   	    uart_write(command_serial_fd,stop_right,5);
            }
void calculate(int dev){
	int max=0x064B;
	int min=0x0578;
	int result; 
	result=max-min;
		
	
}