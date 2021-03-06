//
// Created by zgd on 18-4-20.
//

#include"serial_usb_com.h"
#include <sys/time.h>
#include "generate_pos.h"
#include <iostream>
#include "analy_route.h"
#include "analy.h"
#include "Serial_RT.h"
#include "send_commend.h"
#include <list>
#include <cstring>
#include "run_light.h"
#include "get_light.h"
using namespace std;

int time_tag=0;//飞机悬停时微调和飞行过程中微调用的方法不一样，0为悬停，1为飞行
bool is_hover=false;




void command_thread(){//这个函数是整个程序的核心,完成了，接受uwb数据，生成位置，判断位置，设置分组，发送命令所有任务
    char *com = "/dev/ttyUSB0" ;//读取uwb设备串口名
    char *uart1="/dev/ttyAMA1";//向无人机发送命令串口
    int uart1_para=9600; // 定义

    int len;//buf数组大小
    int out=0 ;//用于判断output数组是否完成了
    int ibx ;//uwb数据中基站的距离
    int icy ;//uwb数据中信号的强度
    int ret ;//用于uwb数据分类，F1是信标数据，1是第一个基站数据，0是第2,3,4....基站的数据，如果是-1就是没有解析出或者没收到数据
    short x=0,y=0,z=0 ;//临时记录定位算法给出的x,y,z
    char rcv_buf[200] ;
    char mac[80] ;//基站mac地址
    int check_num=0;//记录定位数据次数(加在一开始)
    int start_check=0;//起飞后灯管位置正确的次数

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
            int dst=3;
       // }
        if(generate_route(start,dst)&&!route.empty()) {
            cout<<"generate route success"<<endl;
	        cout<<"route_size="<<route.size()<<endl;


            list<int>::iterator it = route.begin();//iteratoræåè·¯åŸç¬¬äžäžªåçŽ 
          take_off();

   //    sleep(5);
//	    sleep(60);
//	send_land();
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
            cout<<" try to lock light"<<endl;

            pthread_mutex_lock(&mutex_light_thread);
            light_thread=true;
            pthread_mutex_unlock(&mutex_light_thread);
            cout<<"change light success"<<endl;
            pthread_create(&run_light_id,NULL,run_light,NULL);//起飞结束后启动图像调整线程
       	    cout<<"create light_thread success"<<endl;
            pthread_create(&adjust_theta_id,NULL,adjustment_theta,NULL);
	        //sleep(10);
            pthread_create(&adjust_thread_id,NULL,adjustment,NULL);//调整线程
	        sleep(5);

	        while (1){//起飞后调整位置如果能飞就执行任务
	            int nowcolor=0;//起飞的时候的灯管颜色
                pthread_mutex_lock(&mutex_colortag);
                nowcolor=colortag;
                pthread_mutex_unlock(&mutex_colortag);

                if(nowcolor==1) {//如果起飞的时候是蓝色
                    pthread_mutex_lock(&mutex_pix);
                    if(abs(pix_x - 320) < 20){
                        start_check++;
                    }
                    else{
                        start_check=0;
                    }
                    pthread_mutex_unlock(&mutex_pix);
                    if(start_check>10){
                        start_check=0;
                        break;
                    }
                    else{
                        usleep(1000*100);
                    }
                }
                if(nowcolor==2){
                    pthread_mutex_lock(&mutex_pix);
                    if(abs(pix_y - 270) < 20){
                        start_check++;
                    }
                    else{
                        start_check=0;
                    }
                    pthread_mutex_unlock(&mutex_pix);
                    if(start_check>10){
                        start_check=0;
                        break;
                    }
                    else{
                        usleep(1000*100);
                    }
                }

	        }
	    //time_tag=1;
//	 send_go_forward();
	// usleep(1000*2000);
//	send_go_back();
            //sleep(60);
          //  send_land();

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


                    rcv_buf[len] = '\0';
                    cout<<rcv_buf<<endl;
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
                            set_info(ret, mac, (float) ((ibx + 1) / 100.0), &out);//把接收到的数据设置到output数组里
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
			             //   cout<<"pix_x="<<pix_x<<"pix_y="<<pix_y<<endl;
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
                                cout<<"group="<<G[current_node][next_node].group<<endl;
                                cout<<"dst_x="<<G[current_node][next_node].dst_x<<endl;
                                cout<<"dst_y="<<G[current_node][next_node].dst_y<<endl;
//                                cout << "X=" << X << endl;
//                                cout<< "Y=" << Y << endl;
                                //然后判断当前坐标
                                //这里dst_x,h和dst_y中其中一个肯定是和灯管坐标一样的,但是命令控制微调和普通的调不一样的话


                                if(it!=(--route.end())) {//如果不是最后降落点
                                    int isArrive;//判断是否到达转弯点
                                    isArrive = generate_command(G[current_node][next_node].dst_x,
                                                                G[current_node][next_node].dst_y,
                                                                G[current_node][next_node].str_x,
                                                                G[current_node][next_node].str_y, X, Y);
                                    cout << "generate command success" << endl;

                                    if (isArrive == 1) {
                                        cout << "arrive" << endl;
                                        check_num++;//检测到一次到达点就check_num+1;
                                    } else {
                                        check_num = 0;
                                        cout << "current=" << current_node << endl;
                                        cout << "next=" << next_node << endl;
                                    }
                                    if (check_num >= 3) {//当符合条件的定位次数大于3的时候则认为到达可以通过微调定位的转弯点了
                                        int ncheck = *(++it);//判断下一个目标位置使用的灯管是不是和原来的一样的
                                        it--;//it减回来
                                        next_path = true;
//                                    send_hover();
//                                    sleep(20);
//                                    send_land();
                                        if (/*it != (--route.end()) &&*/ G[next_node][ncheck].color !=
                                                                     G[current_node][next_node].color) {//如果下一个节点不是终点，查看到下一个线路用的灯管是不是和当前灯管的颜色是否是相同的。如果不相同就微调
                                            cout << "next path =true" << endl;
                                            //is_hover= true;
                                            pthread_mutex_lock(&mutex_colortag);
                                            colortag = 0;
                                            pthread_mutex_unlock(&mutex_colortag);
                                            send_hover();
                                            //sleep(30);
                                            //send_land();
                                            //is_hover=false;
                                            sleep(10);
                                            int turn_check_num = 0;
                                            if (G[current_node][next_node].color == 1) {//如果当前灯管颜色是蓝色，那么先进入红色微调，蓝色应该不用调了
                                                cout << "try to lock colortag" << endl;

                                                pthread_mutex_lock(&mutex_colortag);
                                                colortag = 2;//换成红色微调
                                                pthread_mutex_unlock(&mutex_colortag);
                                                while (turn_check_num < 5) {
                                                    if (abs(pix_y - 270) < 20) {
                                                        turn_check_num++;
                                                    } else turn_check_num = 0;

                                                    if(G[next_node][ncheck].str_y<G[next_node][ncheck].dst_y) {//因为当前是红色灯管微调所以下一条路线肯定是往y方向运动的
                                                        //如果出发节点的y小于目标节点的y那在微调的时候不能向左偏太多
                                                        if (pix_x<100) {
                                                            just_right();
                                                            usleep(1000*2000);
                                                            send_stop_cross();
                                                        }
                                                    }
                                                    else  if(G[next_node][ncheck].str_y>G[next_node][ncheck].dst_y){
                                                        //如果出发点的y大于目标节点的y那在微调的时候不能
                                                        if(pix_x>540){
                                                            just_left();
                                                            usleep(1000*2000);
                                                            send_stop_cross();
                                                        }
                                                    }
                                                    usleep(1000 * 1000);//每一秒检查一次
                                                }
                                            } else if (G[current_node][next_node].color == 2) {

                                                pthread_mutex_lock(&mutex_colortag);
                                                colortag = 1;//换成蓝色微调
                                                pthread_mutex_unlock(&mutex_colortag);
                                                while (turn_check_num < 5) {
                                                    if (abs(pix_x - 320) < 20) {//这里访问像素不锁应该没问题吧
                                                        turn_check_num++;
                                                    } else turn_check_num = 0;
                                                    if(G[next_node][ncheck].str_x<G[next_node][ncheck].dst_x) {//因为当前是红色灯管微调所以下一条路线肯定是往y方向运动的
                                                        //如果出发节点的x小于目标节点的x那在微调的时候不能向后太多
                                                        if (pix_y<80) {
                                                            just_forward();
                                                            usleep(1000*2000);
                                                            send_stop_front();
                                                        }
                                                    }
                                                    else  if(G[next_node][ncheck].str_x>G[next_node][ncheck].dst_x){
                                                        //如果出发点的x大于目标节点的x那在微调的时候不能向前太多
                                                        if(pix_y>400){
                                                            just_back();
                                                            usleep(1000*2000);
                                                            send_stop_front();
                                                        }
                                                    }
                                                    usleep(1000 * 1000);//每一秒检查一次
                                                }
                                            }

                                            /* //坐标判断到达中心点,悬停
                                             sleep(5);//停止五秒后开始调转弯点
                                             pthread_mutex_lock(&mutex_colortag);
                                              colortag=3;
                                              pthread_mutex_unlock(&mutex_colortag);
                                             while(1){

                                                 cout<<"  adjusting "<<endl;
                                                 pthread_mutex_lock(&mutex_colortag);
                                                // colortag=3;
                                                 cout<<"colortag="<<colortag;
                                                 pthread_mutex_unlock(&mutex_colortag);

                                                 if(abs(pix_x-320)<20&&abs(pix_y-240)<10){
                                                     turn_check_num++;
                                                     if(turn_check_num>5) {
                                                         pthread_mutex_lock(&mutex_colortag);
                                                         colortag = 0;
                                                         pthread_mutex_unlock(&mutex_colortag);
                                                         break;
                                                     }
                                                 }
                                                 else turn_check_num=0;
                                                 usleep(1000*500);
                                             }*/
                                            //sleep(1);
                                            check_num = 0;
                                        }
                                    }
                                }
                                else if(it==(--route.end())){//如果是最后降落点
                                    int isReady;
                                    isReady=go_to_land(G[current_node][next_node].dst_x,
                                                             G[current_node][next_node].dst_y,
                                                             G[current_node][next_node].str_x,
                                                             G[current_node][next_node].str_y, X, Y);//移动到降落点的命令，暂时先用路径上的指令方式

                                    if (isReady == 1) {
                                        cout << "arrive" << endl;
                                        check_num++;//检测到一次到达点就check_num+1;
                                    } else {
                                        check_num = 0;
                                     //   cout << "current=" << current_node << endl;
                                     //   cout << "next=" << next_node << endl;
                                    }
                                    if(check_num>=5){
                                        next_path = true;
                                        sleep(5);
                                        check_num = 0;
                                    }
                                }
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

                cout<<"land_success,light stop"<<endl;
                sleep(10000);
            }
        }
         sleep(1);
    return ;
    }



//飞机微调线程函数，通过图像处理得到的结果来进行飞机的微调
void *adjustment(void* arg){
    ofstream fout;
    fout.open("log.txt",ios::out|ios::app);//日志
    int threshold=100;
    //设置线程运行cpu
    cpu_set_t m_mask;
    CPU_ZERO(&m_mask);
    CPU_SET(3,&m_mask);
    if (pthread_setaffinity_np(pthread_self(),sizeof(m_mask),&m_mask)<0)
    {
        fprintf(stderr,"set thread affinity failed\n");
    }
    /*int tm;//记录发送指令的间隔时间次数，时间间隔要通过测试确定
	int tm_sleep;//休停时间*/

    int ad_pix_x,ad_pix_y;//临时记录像素值
	int color;//临时记录颜色
    /*    //double t1 = (double)getTickCount();
    //time_t timep;
    //time (&timep);
    //fout<<current<<endl;*/

    int adj_right_num=0;
    int adj_left_num=0;//记录调整的次数如果次数大于20就先停一会再调整
    int adj_front_num=0;
    int adj_back_num=0;

    while(1){
	    bool do_adj=false;
        pthread_mutex_lock(&mutex_light_thread);
        do_adj=light_thread;//读取灯光是否运行标记位
        pthread_mutex_unlock(&mutex_light_thread);
        if(do_adj==false){
            break;
        }
       /* double t = (double)getTickCount();
        //t1 = ((double)getTickCount() - t1) / getTickFrequency();
/*	//if(time_tag==0){//悬停
//	tm=10;//每10毫秒读取一此pix
//	tm_sleep=10;
//	}
//	else if(time_tag==1){
//	tm=20;
//	tm_sleep=10;
//	}
//
//	else{
//	tm=20;
//	cout<<"time_tag error"<<endl;
//	}
       // cout<<"t1="<<t1<<endl;*/
        //读取colortag标记位
        pthread_mutex_lock(&mutex_colortag);
        color=colortag;
        pthread_mutex_unlock(&mutex_colortag);



        //读取像素值
        pthread_mutex_lock(&mutex_pix);
	    ad_pix_x=pix_x;
	    ad_pix_y=pix_y;
	    pthread_mutex_unlock(&mutex_pix);
	    cout<<"pix_x="<<ad_pix_x<<"pix_y"<<ad_pix_y<<endl;
        //fout<<"pix_x="<<ad_pix_x<<"pix_y"<<ad_pix_y<<endl;
        /*//	if(ad_pix_x==0&&ad_pix_y==0){
//
//		usleep(1000*300);
//		continue;
//	}*/


	    //t = ((double)getTickCount() - t) / getTickFrequency();
   //     cout<<"color="<<color<<endl;
        double t = (double)getTickCount();


	if(color==1/*&&!is_hover*/)//蓝色
	{
        if((adj_left_num>=30||adj_right_num>=30)&&(abs(pix_x-320)<threshold)){
            send_stop_cross();
            usleep(1000*500);
            adj_left_num=0;
            adj_right_num=0;
            continue;
        }
        // double t = (double)getTickCount();
		if(ad_pix_x>340&&ad_pix_x<640){
		    send_go_left(ad_pix_x,ad_pix_y);
		    adj_left_num++;
		    adj_right_num=0;
		    cout<<"go_left"<<endl;
//	    	fout<<"go_left"<<endl;

		    }
		else if(ad_pix_x>0&&ad_pix_x<300){
		    send_go_right(ad_pix_x,ad_pix_y);
		    adj_right_num++;
		    adj_left_num=0;
		    cout<<"go_right"<<endl;
//		    fout<<"go_right"<<endl;

		    }

		else {//在中间就停止
		    send_stop_cross();
		    adj_left_num=0;
		    adj_right_num=0;
		    cout<<"stop_cross"<<endl;
		    usleep(1000*500);
		}
        t = ((double)getTickCount() - t) / getTickFrequency();
        //cout << "times passed in seconds: " << t << endl;
         // t1 = (double)getTickCount();
	}

	else if(color==2/*&&!is_hover*/)//紅色
	{

	    if((adj_front_num>=30||adj_back_num>=30)&&(abs(ad_pix_y-270)<threshold)){
            send_stop_front();
            usleep(1000*500);
            adj_front_num=0;
            adj_back_num=0;
            continue;
	    }
	    if(ad_pix_y>290&&ad_pix_y<480){
		    send_go_back();
		    adj_back_num++;
		    adj_front_num=0;
		    cout<<"go_back"<<endl;
//          fout<<"go_back"<<endl;
	    }
	    else if(ad_pix_y>0&&ad_pix_y<250){
    		send_go_forward();
    		adj_front_num++;
    		adj_back_num=0;
	    	cout<<"go_forward"<<endl;
//		    cout<<"go_front"<<endl;
//          fout<<"go_front"<<endl;
	    }
	    else {
	        adj_back_num=0;
	        adj_front_num=0;
	        send_stop_front();
	        cout<<"stop"<<endl;
	        usleep(1000*500);
	    }
	}
//	else if(is_hover){
//	    //send_hover();
//	    sleep(1);
//	}
	else if(color==3){
	    cout<<"send_adjust"<<endl;
	    send_adj(ad_pix_x,ad_pix_y);//转弯点两种颜色微调
	}
	else if(color==0){//转弯后调整结束后悬停一段时间
	    adj_front_num=0;
	    adj_back_num=0;
	    adj_left_num=0;
	    adj_right_num=0;
	    send_hover();
	}
	else{
	    send_land();//意外降落；
	}
	}
    fout.close();
}


void *adjustment_theta(void* arg){
    double ad_theta;
    while(1) {
        bool do_adj;
        pthread_mutex_lock(&mutex_light_thread);
        do_adj=light_thread;
        pthread_mutex_unlock(&mutex_light_thread);

        if(do_adj==false){
            break;
        }
        pthread_mutex_lock(&mutex_theta);
        ad_theta = theta;
        pthread_mutex_unlock(&mutex_theta);
       // cout<<"theta="<<ad_theta<<endl;
        if (ad_theta > 5 && ad_theta < 175) {
           // cout<<"its theta hold"<<endl;
            theta_hold(theta);
        } else {
            send_stop_rotation();
        }
    }
}


void *pitch_adjust(void *arg){//前后移动的微调
        //飞机微调线程函数，通过图像处理得到的结果来进行飞机的微调
        ofstream fout;
        fout.open("log_cross.txt",ios::out|ios::app);//日志

        //设置线程运行cpu
        cpu_set_t m_mask;
        CPU_ZERO(&m_mask);
        CPU_SET(3,&m_mask);
        if (pthread_setaffinity_np(pthread_self(),sizeof(m_mask),&m_mask)<0)
        {
            fprintf(stderr,"set thread affinity failed\n");
        }
        /*int tm;//记录发送指令的间隔时间次数，时间间隔要通过测试确定
        int tm_sleep;//休停时间*/
        int ad_pix_y;//临时记录y像素值
        int color;//临时记录颜色
        /*    //double t1 = (double)getTickCount();
        //time_t timep;
        //time (&timep);
        //fout<<current<<endl;*/
        while(1){//循环调整
            bool do_adj=false;
            pthread_mutex_lock(&mutex_light_thread);
            do_adj=light_thread;//读取灯光是否运行标记位
            pthread_mutex_unlock(&mutex_light_thread);
            if(do_adj==false){//如果灯光不运行跳出循环
                break;
            }
            /* double t = (double)getTickCount();
             //t1 = ((double)getTickCount() - t1) / getTickFrequency();
            // cout<<"t1="<<t1<<endl;*/
            //读取像素值只需要读
            pthread_mutex_lock(&mutex_pix_y);
            ad_pix_y=pix_y;
            pthread_mutex_unlock(&mutex_pix_y);
            cout<<"pix_y"<<ad_pix_y<<endl;
            //读取colortag标记位
            pthread_mutex_lock(&mutex_colortag);
            color=colortag;
            pthread_mutex_unlock(&mutex_colortag);
            //t = ((double)getTickCount() - t) / getTickFrequency();
            cout<<"color="<<color<<endl;
            double t = (double)getTickCount();
            if(color==2||color==3)//colortag=2或者colortag=3（两个一起微调）
            {
                if(ad_pix_y>290&&ad_pix_y<480){
                    send_go_back();
                    cout<<"go_back"<<endl;
                }
                else if(ad_pix_y>0&&ad_pix_y<250){
                    send_go_forward();
                    cout<<"go_forward"<<endl;
                }
                else {
                    send_stop_front();
                    cout<<"stop"<<endl;
                    usleep(1000*500);
                }
            }
            else if(color==0){//color=0就悬停
                send_hover();
            }
            else{
                send_land();//意外降落；
            }
        }
        fout.close();
    }

//飞机微调线程函数，通过图像处理得到的结果来进行飞机的微调
void *raw_adjust(void* arg){
    ofstream fout;
    fout.open("log.txt",ios::out|ios::app);//日志

    //设置线程运行cpu
    cpu_set_t m_mask;
    CPU_ZERO(&m_mask);
    CPU_SET(3,&m_mask);
    if (pthread_setaffinity_np(pthread_self(),sizeof(m_mask),&m_mask)<0)
    {
        fprintf(stderr,"set thread affinity failed\n");
    }


    int ad_pix_x,ad_pix_y;//临时记录像素值
    int color;//临时记录颜色



    while(1){
        bool do_adj=false;
        pthread_mutex_lock(&mutex_light_thread);
        do_adj=light_thread;//读取灯光是否运行标记位
        pthread_mutex_unlock(&mutex_light_thread);
        if(do_adj==false){
            break;
        }


        //读取colortag标记位
        pthread_mutex_lock(&mutex_colortag);
        color=colortag;
        pthread_mutex_unlock(&mutex_colortag);
        //t = ((double)getTickCount() - t) / getTickFrequency();
        cout<<"color="<<color<<endl;
        double t = (double)getTickCount();

        if(color==1&&!is_hover)//蓝色
        {
            // double t = (double)getTickCount();
            if(ad_pix_x>340&&ad_pix_x<640){
                send_go_left(ad_pix_x,ad_pix_y);
                cout<<"go_left"<<endl;
//		fout<<"go_left"<<endl;
                /*	//while(tm--){//每次停10毫秒就检测一次图像结果
                    //usleep(1000*tm);
            //		pthread_mutex_lock(&mutex_pix);
            //        ad_pix_x=pix_x;
            //		ad_pix_y=pix_y;
            //		pthread_mutex_unlock(&mutex_pix);

            //		if(ad_pix_x>300&&ad_pix_x<340)break;
            //		}
                    //send_go_right();
                    //usleep(1000*25);
                    //send_stop_cross();
                    //usleep(1000*tm_sleep);
                    //cout<<"and stop"<<endl;*/
            }
            else if(ad_pix_x>0&&ad_pix_x<300){
                send_go_right(ad_pix_x,ad_pix_y);
                cout<<"go_right"<<endl;
//		fout<<"go_right"<<endl;
/*//		while(tm--){
		//usleep(1000*tm);
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
		//send_stop_cross();
		//usleep(1000*tm_sleep);
		//cout<<"and stop"<<endl;*/
            }
                // usleep(1000*50);
            else {//在中间就停止
                send_stop_cross();
                cout<<"stop_cross"<<endl;
                usleep(1000*500);
            }
            t = ((double)getTickCount() - t) / getTickFrequency();
            //cout << "times passed in seconds: " << t << endl;
            // t1 = (double)getTickCount();
        }

        else if(color==2&&!is_hover)//紅色
        {
            if(ad_pix_y>290&&ad_pix_y<480){
                send_go_back();
                cout<<"go_back"<<endl;
//      fout<<"go_back"<<endl;
            }
            else if(ad_pix_y>0&&ad_pix_y<250){
                send_go_forward();
                cout<<"go_forward"<<endl;
//		cout<<"go_front"<<endl;
//        fout<<"go_front"<<endl;
            }
            else {
                send_stop_front();
                cout<<"stop"<<endl;
                usleep(1000*500);
            }
        }
        else if(is_hover){
            send_hover();
            //sleep(10);
        }
        else if(color==3){
            cout<<"send_adjust"<<endl;
            send_adj(ad_pix_x,ad_pix_y);//转弯点两种颜色微调
        }
        else if(color==0){//转弯后调整结束后悬停一段时间
            send_hover();
        }
        else{
            send_land();//意外降落；
        }
    }
    fout.close();
}


void *uwb_command(void* arg){
    //设置线程运行cpu
    cpu_set_t m_mask;
    CPU_ZERO(&m_mask);
    CPU_SET(0,&m_mask);
    if (pthread_setaffinity_np(pthread_self(),sizeof(m_mask),&m_mask)<0)
    {
        fprintf(stderr,"set thread affinity failed\n");
    }


    int len;//buf数组大小
    int out=0 ;//用于判断output数组是否完成了
    int ibx ;//uwb数据中基站的距离
    int icy ;//uwb数据中信号的强度
    int ret ;//用于uwb数据分类，F1是信标数据，1是第一个基站数据，0是第2,3,4....基站的数据，如果是-1就是没有解析出或者没收到数据
    short x=0,y=0,z=0 ;//临时记录定位算法给出的x,y,z
    char rcv_buf[200] ;
    char mac[80] ;//基站mac地址
    int check_num=0;//记录定位数据次数(加在一开始)
    int start_check=0;//起飞后灯管位置正确的次数

    while(1){


    }



}
