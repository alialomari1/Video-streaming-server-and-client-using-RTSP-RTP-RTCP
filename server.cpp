// All standard libraries here:
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <time.h>

#include <string>
#include <cstring>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <fstream>
#include <filesystem>

#include <pthread.h>
using namespace std;

#include "lib/packets.h"
// Global shared varibles here:
int g_current_img = 1;
bool g_play, g_teardown =false;
short g_client_port=12345;  // client rtp  listening port

//RTCP Vars
short g_rtcp_port= 12346;   // server rtcp listening port
int g_octet_sent_counter=0, g_pkt_sent_counter=0;
struct RTPPacket g_rtp_current_pkt;
int g_lost=0;               // number of rtp lost
pthread_mutex_t lock_ctr, lock_jitter, lock_lost;


//client var, so ignore
float R[501]={0}, S[501]={0}, Jitter_dp[501] = {0}; // Recieve times, and Send times, used to calculate jitter


// All user libraries here:
#include "lib/rtcp.cpp"
#include "lib/rtp.cpp"
#include "lib/rtsp.cpp"

int RTSPPacket::cseq = 0;

int main(int argc, char *argv[])
{
    struct RTCPPacket rtcp;
    rtcp.common.pt = 201;

    ofstream *rtcp_logger= new ofstream("logs/rtcp_server.log", ios_base::app);
    int *pkt_type = new int(0);
    // rtsp
    pthread_t rtsp_server_thread, rtp_server_thread,rtcp_listener_thread, rtcp_sender_thread;
    pthread_create(&rtsp_server_thread, NULL, rtsp_server, NULL);
    pthread_create(&rtp_server_thread, NULL, rtp_server, NULL);
    pthread_create(&rtcp_listener_thread, NULL, rtcp_listener, rtcp_logger);
    pthread_create(&rtcp_sender_thread, NULL, rtcp_sender, pkt_type); 


    // join threads
    pthread_join(rtsp_server_thread, NULL);
    pthread_join(rtp_server_thread, NULL);
    pthread_join(rtcp_listener_thread, NULL);
    pthread_join(rtcp_sender_thread, NULL);
}