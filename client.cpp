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
bool g_play = 1, g_teardown = false;
short g_client_port = 12345; // client rtp  listening port
short g_rtcp_port = 12347;   // client rtcp listening port
struct RTPPacket g_rtp_current_pkt;
float R[501] = {0}, S[501] = {0}, Jitter_dp[501] = {0}; // Recieve times, and Send times, used to calculate jitter

// server side vars, ignore them {
short g_octet_sent_counter = 0, g_pkt_sent_counter = 0;
int g_lost = 1;
pthread_mutex_t lock_ctr, lock_jitter, lock_lost;

// } server side vars, ignore them

// All user  libraries here:
#include "lib/CImg.h"
#include "lib/rtcp.cpp"
#include "lib/rtp.cpp"
#include "lib/rtsp.cpp"

void client_player();
void controll(cimg_library::CImgDisplay *);
void get_last_two_logs(string &);

int RTSPPacket::cseq = 0;
pthread_t rtp_client_thread, rtcp_client_thread, rtcp_listener_thread, rtcp_sender_thread;
int main(int argc, char *argv[])
{

    lock_ctr = lock_jitter = lock_lost = PTHREAD_MUTEX_INITIALIZER;
    struct RTCPPacket rtcp;
    ofstream *rtcp_logger = new ofstream("logs/rtcp_client.log", ios_base::app);
    int *pkt_type = new int(1);

    pthread_create(&rtp_client_thread, NULL, rtp_client, NULL);
    pthread_create(&rtcp_listener_thread, NULL, rtcp_listener, rtcp_logger);
    pthread_create(&rtcp_sender_thread, NULL, rtcp_sender, pkt_type);

    // rtsp
    usleep(1000000); // wait for rtp client to start
    rtsp_client(SETUP);
    usleep(1000000); // wait for first imgs (buffering)
    client_player();
}
using namespace cimg_library;

void client_player()
{
    filesystem::create_directory("client_buffer");
    CImgDisplay disp;
    disp.resize(768, 576 + 400);
    while (g_current_img <= 500)
    {
        usleep(45000);
        if (g_teardown)
        {
            // wait for all threads to finish (they need to send teardown packets)
            pthread_join(rtp_client_thread, NULL);
            pthread_join(rtcp_listener_thread, NULL);
            pthread_join(rtcp_sender_thread, NULL);

            filesystem::remove_all("client_buffer");
            exit(0);
        }

        char name[200];

        sprintf(name, "client_buffer/image%03d.jpg", g_current_img);
        // first, check if file exist and not empty
        if (!std::filesystem::exists(name) ||
            std::filesystem::exists(name) && !std::filesystem::file_size(name))
        {
            usleep(100000);
            cout << "packet lost:" << g_current_img << endl;
            g_current_img++;
            continue;
        }

        CImg<unsigned char> img(name);
        img.resize_doubleXY(); // new size(768x576)
        CImg<unsigned char> frame(disp.width(), disp.height(), 1, 3, 0);
        frame.draw_image(0, 0, 0, 0, img, 1);

        // RTCP Part {
        string text = "\t\t\t" + to_string(g_current_img) + "/500" + "\n--+( RTCP information )+--";
        frame.draw_text(768 / 2 - sizeof(text) - 30, 576, &text[0], "white", 1, 24);

        // Server side (Receiving RR & Bye)
        ifstream rtcp_server("logs/rtcp_server.log", ios_base::app);
        char chs[800000] = {0};
        rtcp_server.read(chs, 800000);
        text = string(chs) + " ";
        get_last_two_logs(text);
        text = "Server Log:\n~~~~~~~\n\n" + text;
        frame.draw_text(100, 576 + 37, &text[0], "white", 1, 24);

        // Client side (Receiving SR & Bye)
        ifstream rtcp_client("logs/rtcp_client.log", ios_base::app);
        strncpy(chs, "", 800000);
        rtcp_client.read(chs, 800000);
        text = string(chs) + " ";
        get_last_two_logs(text);
        text = "Client Log:\n~~~~~~\n\n" + text;

        frame.draw_text(450, 576 + 37, &text[0], "white", 1, 24);

        // } RTCP PART
        disp = frame;

        if (g_play)
            g_current_img++;

        controll(&disp);
    }
    filesystem::remove_all("client_buffer");
    exit(0);
}

void controll(CImgDisplay *disp)
{
    if (disp->is_keySPACE())
    {
        g_play = !g_play;
        if (g_play)
        {
            rtsp_client(PLAY);
            cout << "play" << endl;
        }
        else
        {
            rtsp_client(PAUSE);
            cout << "pause" << endl;
        }
        usleep(1000000); // wait before next keystroke
    }
    else if (disp->is_keyQ() || disp->is_keyESC() || disp->is_closed())
    {
        g_teardown = true;
        cout << "teardown" << endl;
        rtsp_client(TEARDOWN);
        void *pkt_type = new int(2);
        rtcp_sender(pkt_type);
        sleep(2);
        exit(0);
    }
    else if (disp->is_keyR())
    {
        g_current_img = 1;
        cout << "reset" << endl;
        usleep(1000000); // wait before next keystroke
    }
    else if (disp->is_keyARROWLEFT())
    {
        g_current_img = g_current_img - 50 > 0 ? g_current_img - 50 : 1;
        cout << "left" << endl;
        usleep(500000); // wait before next keystroke
    }
    else if (disp->is_keyARROWRIGHT())
    {
        g_current_img = g_current_img + 50 < 500 ? g_current_img + 50 : 500;
        cout << "right" << endl;
        usleep(500000); // wait before next keystroke
    }
}

void get_last_two_logs(string &s)
{
    int last = 0;
    for (int i = s.length(); i >= 0; --i)
    {
        if (s[i] == '>')
        {
            last++;
        }
        if (last == 2)
        {
            s = s.substr(i);
            break;
        }
    }
}