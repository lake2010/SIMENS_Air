//
// Created by zgd on 18-4-20.
//

#include"serial_usb_com.h"

#include "generate_pos.h"
#include <iostream>
#include "analy_route.h"
#include "analy.h"
#include "Serial_RT.h"
#include "send_commend.h"
#include <list>
#include <cstring>
#include "run_light.h"
using namespace std;

int time_tag=0;//飞机悬停时微调和飞行过程中微调用的方法不一样，0为悬停，1为飞行

void command_thread(){//这个函数是整个程序的核心,完成了，接受uwb数据，生成位置，判断位置，设置分组，发送命令所有任务
    char *com = "/dev/ttyUSB0" ;//读取uwb设备串口名
    char *uart1="/dev/ttyAMA1";//向无人机发送命令串口
    int uart1_para=9600;

    int len;//buf数组大小
    int out=0 ;//用于判断output数组是否完成了
    int ibx ;//uwb数据中基站的距离
    int icy ;//uwb数据中信号的强度
    int ret ;//用于uwb数据分类，F1是信标数据，1是第一个基站数据，0是第2,3,4....基站的数据，如果是-1就是没有解析出或者没收到数据
    short x=0,y=0,z=0 ;//临时记录定位算法给出的x,y,z
    char rcv_buf[200] ;
    char mac[80] ;//基站mac地址
    int check_num=0;//记录定位数据次数(加在一开始)


    while(open_serial(com)==-1){//打开uwb串口
        sleep(10) ;
    }
    while (uart_open(command_serial_fd,uart1,uart1_para)==-1){
        sleep(1);
    }
//    while(uart_set(command_serial_fd,uart1_para,0,8,'N',1) == -1){
//        sleep(1);
//    }

    int t=1;
    while(/*监听tcp接受路线任务*/t) {
        //if(收到起点,终点 start,dst){
            int start=0;
            int dst=1;
       // }
        if(generate_route(start,dst)&&!route.empty()) {
            cout<<"generate route success"<<endl;
	cout<<"route_size="<<route.size()<<endl;
            list<int>::iterator it = route.begin();//iteratoræåè·¯åŸç¬¬äžäžªåçŽ 
            take_off();

       sleep(2);         


	//    sleep(60);

	//send_land();
//            uart_write(command_serial_fd,switch_hover,5);
            cout<<"take off success"<<endl;


            //发送起飞命令,悬停命令,还要解析无人机给的高度数据.
            int str, nstr;//用于确定第一个灯管的颜色
            str=*it;
            nstr=*(++it);
            pthread_mutex_lock(&mutex_colortag);
            colortag=G[str][nstr].color;
            pthread_mutex_unlock(&mutex_colortag);
            it--;//把it减回来


            pthread_mutex_lock(&mutex_light_thread);
            light_thread=true;
            pthread_mutex_unlock(&mutex_light_thread);

            pthread_create(&run_light_id,NULL,run_light,NULL);//起飞结束后启动图像调整线程
       	    
	    sleep(10);
	    
	    pthread_create(&adjust_thread_id,NULL,adjustment,NULL);//调整线程
	    sleep(25);
	    time_tag=1;	
//	 send_go_forward();
//	send_go_back();
//	sleep(60);
//	send_land();			
            while (it != (--route.end())) {//只要当前it不是最后一个节点就循环
                int current_node, next_node;//当前节点编号和下一个节点编号
                current_node = *it;
                next_node = *(++it);
		cout<<"current_node="<<current_node<<endl;
		cout<<"next_node="<<next_node<<endl;
		cout<<"group="<<G[current_node][next_node].group<<endl;
                serial_set_group(G[current_node][next_node].group);

                pthread_mutex_lock(&mutex_colortag);
                colortag=G[current_node][next_node].color;
                pthread_mutex_unlock(&mutex_colortag);

                bool next_path = false;//
                while (!next_path) {
                    len = 100;
                    usleep(10);
                    memset(rcv_buf, '\0', len);
                    while(serial_read(rcv_buf, &len)==-1){
                        cout<<"read failed"<<endl;
                        usleep(1);
                    };//读取uwb数据,如果失败就再读
//		    cout<<rcv_buf<<endl;
                    rcv_buf[len] = '\0';
                   // cout<<rcv_buf<<endl;
                    if (len > 0 && len < 80) {

                        memset(mac, '\0', 50);

                        ret = get_info(rcv_buf, len, mac, &ibx, &icy);
                        if (ret == -1) {
                            cout<<"read info failed"<<endl;
                            continue;//如果分析数据失败结束本次循环，进行下一次循环，再次读取数据
                        }
                        bool set_pos_success=false;//记录output数组数据是否设置完成
                        //二维计算方法
                        if (G[current_node][next_node].dimension == 2) {
                            out = 0;//用于记录是否数据设置成功，
                            set_info(ret, mac, (float) ((ibx + 1) / 100.0), &out);//äºç»Žçæ¹åŒè·åå°åæ å¹¶èµåŒïŒè¿éå§ibxåç±³åäœæ¢æç±³
                            if (out == 0xff)//设置输出完成
                            {
                                x = (short) (output[0] * 100);
                                y = (short) (output[1] * 100);
                                z = (short) (output[2] / 20);
                                set_pos_success=true;
                            }
                            else set_pos_success=false;

                        }
                            //一维计算方法
                        else if (G[current_node][next_node].dimension == 1) {
                            out = 0;
                            set_one_dimension(ret, mac, (float) ((ibx + 1) / 100.0), &out);
                            if (out == 0xff) {
                                x = (short) (output[0] * 100);
                                y = (short) (output[1] * 100);
                                z = (short) (output[2] / 20);
                                set_pos_success=true;
                            }
                            else set_pos_success= false;
                        }
			//cout<<"pix_x="<<pix_x<<"pix_y="<<pix_y<<endl;
                        if(set_pos_success==true) {//如果uwb数据定位完成，启用灯管定位
                         

			cout<<"color ="<<G[current_node][next_node].color<<endl;
			cout<<"pix_x="<<pix_x<<"pix_y="<<pix_y<<endl;
                            //先对像素坐标进行判断一下有没有出错
                            if((pix_x>=10&&pix_x<=630)||(pix_y>=10&&pix_y<=470)) {
                                //判断灯管修改坐标值
                                pthread_mutex_lock(&mutex_pix);
                                if (G[current_node][next_node].color == 1) {//蓝色灯管改变y方向的值
                                    y = G[current_node][next_node].light_pos + ((pix_x - 320) * 10) / 15;
					cout<<"blue color success"<<endl;
                                } else if (G[current_node][next_node].color == 2) {//红色灯管改变x方向的值
                                    x = G[current_node][next_node].light_pos + ((pix_y - 240) * 10) / 15;
				    cout<<"red_light_set_success"<<endl;
					cout<<"light_pos="<<G[current_node][next_node].light_pos<<endl;
					
                                } else cout << "color set error" << endl;//这里后面可以写到日志里
                                pthread_mutex_unlock(&mutex_pix);
                            }
                            //判断是否在范围内如果在范围内就给全局的X,Y
                            if ((x >= 0) && (y >= 0) && (x < maxX) && (y < maxY)) {
                                X = x;
                                Y = y;
                                printf("X=%d,Y=%d\r\n",X,Y);
//                                cout << "X=" << X << endl;
//                                cout<< "Y=" << Y << endl;
                                //然后判断当前坐标
                                //这里dst_x,h和dst_y中其中一个肯定是和灯管坐标一样的,但是命令控制微调和普通的调不一样的话
                                int isArrive;
                                isArrive=generate_command( G[current_node][next_node].dst_x,G[current_node][next_node].dst_y, G[current_node][next_node].str_x, G[current_node][next_node].str_y,X,Y);
                                if(isArrive==1) {

                                    check_num++;//检测到一次到达点就check_num+1;

                                }
			        else{
                                    check_num=0;
			            cout<<"current="<<current_node<<endl;
			            cout<<"next="<<next_node<<endl;
					            }
                             if(check_num>=3){
                                    next_path=true;
                                    send_hover();//坐标判断到达中心点,悬停
				    	
				    //pthread_mutex_lock(&mutex_colortag);
                		    //colortag=3;
                		    //pthread_mutex_unlock(&mutex_colortag);
				/*while(1){

				cout<<" changing "<<endl;
				if(abs(pix_x-360)<10&&abs(pix_y-240)<10){
					   pthread_mutex_lock(&mutex_colortag);
               				   colortag=0;
                			 pthread_mutex_unlock(&mutex_colortag);
	     						break;								
					}
				}*/
                                    sleep(30);
				//图像微调
                                    check_num=0;
                            }

                               /* if (G[current_node][next_node].dst_x == G[current_node][next_node].light_pos) {
                                    if (abs(X - G[current_node][next_node].dst_x) > 10) {
                                        //xæ¹åäžåŸ®è°åœä»€
                                    }
                                    if (abs(Y - G[current_node][next_node].dst_y) > 10) {
                                        //yæ¹åäžç§»åšåœä»€
                                    }
                                } else {
                                    if (abs(X - G[current_node][next_node].dst_x) > 10) {
                                        //xæ¹åäžç§»åšåœä»€
                                    }
                                    if (abs(Y - G[current_node][next_node].dst_y) > 10 {
                                        // yæ¹åäžåŸ®è°åœä»€
                                    }
                                }*/

                            }
                        }
                    }
                }
            }

            //路线任务完成
            //下降

                send_land();
		cout<<"land success"<<endl;
                pthread_mutex_lock(&mutex_light_thread);
                light_thread=false;
                pthread_mutex_unlock(&mutex_light_thread);
                sleep(1);
            }
        }
         sleep(1);
    return ;
    }










//飞机微调线程函数，通过图像处理得到的结果来进行飞机的微调

void *adjustment(void* arg){
	int tm;//记录发送指令的间隔时间次数，时间间隔要通过测试确定
	int tm_sleep;//休停时间
	int ad_pix_x,ad_pix_y;//临时记录像素值
	int color;//临时记录颜色
	
	while(light_thread){
	if(time_tag==0){//悬停
	tm=10;//每10毫秒读取一此pix
	tm_sleep=10;
	}
	else if(time_tag==1){
	tm=20;
	tm_sleep=10;
	}

	else{
	tm=20;
	cout<<"time_tag error"<<endl;
	}
	pthread_mutex_lock(&mutex_pix);

	ad_pix_x=pix_x;
	ad_pix_y=pix_y;

	pthread_mutex_unlock(&mutex_pix);
	cout<<"pix_x="<<ad_pix_x<<"pix_y"<<ad_pix_y<<endl;
//	if(ad_pix_x==0&&ad_pix_y==0){
//
//		usleep(1000*300);
//		continue;
//	}
    pthread_mutex_lock(&mutex_colortag);
        color=colortag;
	pthread_mutex_unlock(&mutex_colortag);
	if(color==1)//蓝色
	{
		if(ad_pix_x>340&&ad_pix_x<640){
		send_go_left();
		cout<<"go_left"<<endl;
		//while(tm--){//每次停10毫秒就检测一次图像结果
		usleep(1000*tm);
//		pthread_mutex_lock(&mutex_pix);
//        ad_pix_x=pix_x;
//		ad_pix_y=pix_y;
//		pthread_mutex_unlock(&mutex_pix);

//		if(ad_pix_x>300&&ad_pix_x<340)break;
//		}
		//send_go_right();
		//usleep(1000*25);
		send_stop_cross();
		usleep(1000*tm_sleep);
		cout<<"and stop"<<endl;
		}
		else if(ad_pix_x>0&&ad_pix_x<300){
		send_go_right();
		cout<<"go_right"<<endl;
//		while(tm--){
		usleep(1000*tm);
//		pthread_mutex_lock(&mutex_pix);
//	        ad_pix_x=pix_x;
//		ad_pix_y=pix_y;
//		pix_x=0;
//		pix_y=0;
//		pthread_mutex_unlock(&mutex_pix);
//		if(ad_pix_x>300&&ad_pix_x<340)break;
//		}
		//send_go_left();
		//usleep(1000*25);
		send_stop_cross();
		usleep(1000*tm_sleep);
		cout<<"and stop"<<endl;
		}
		// usleep(1000*50);
		else {
		send_stop_cross();
		cout<<"stop"<<endl;
		usleep(1000*100);	
		}
	}	
	if(color==2)//紅色
	{
	if(ad_pix_y>250&&ad_pix_y<480){
		send_go_back();
                cout<<"go_back"<<endl;
		usleep(1000*300);
		send_stop_front();								
	}
	else if(ad_pix_y>0&&ad_pix_y<230){
		send_go_forward();
		cout<<"go_front"<<endl;
		usleep(1000*300);
		send_stop_front();
	}
	else {
	send_stop_front();
	cout<<"stop"<<endl;	
	}		
        }
    }
}