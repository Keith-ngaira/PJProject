#include <chrono>
#include <iostream>
#include <openssl/ssl.h>
#include <pjsua2.hpp>
#include <pj/config_site.h>
#include <pjsip.h>
#include <thread>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjsua-lib/pjsua.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <signal.h>

using namespace pj;
using namespace std;

// Derived Account Class for Registration and Managing Calls
class MyAccount : public Account {
public:
    MyAccount() {}

    virtual void onRegState(OnRegStateParam& prm) override {
        AccountInfo ai = getInfo();
        if (ai.regIsActive) {
            cout << "[INFO] Successfully registered with the SIP server!" << endl;
        }
        else {
            cout << "[ERROR] Registration failed." << endl;
        }
    }

    virtual void onIncomingCall(OnIncomingCallParam& iprm) override {
        Call* call = new Call(*this, iprm.callId);
        CallOpParam prm;
        prm.statusCode = PJSIP_SC_OK;
        call->answer(prm);
        cout << "[INFO] Incoming call received and answered." << endl;
    }
};

// Derived Call Class for Managing Call States
class MyCall : public Call {
public:
    MyCall(Account& acc) : Call(acc) {}

    virtual void onCallState(OnCallStateParam& prm) override {
        CallInfo ci = getInfo();
        cout << "[INFO] Call state changed: " << ci.stateText << endl;

        if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
            cout << "[INFO] Call disconnected. Cleaning up." << endl;
            delete this;
        }
    }
};

// Helper function to create and register an account
MyAccount* setupAccount(const string& idUri, const string& registrarUri, const string& username, const string& password) {
    AccountConfig acc_cfg;
    acc_cfg.idUri = idUri;
    acc_cfg.regConfig.registrarUri = registrarUri;
    acc_cfg.sipConfig.authCreds.push_back(AuthCredInfo("digest", "*", username, 0, password));

    MyAccount* acc = new MyAccount();
    acc->create(acc_cfg);

    return acc;
}

// Thread-safe keep-alive mechanism
void keepAlive(MyAccount* acc) {
    pj_thread_desc desc;
    pj_thread_t* pj_thread;

    if (!pj_thread_is_registered()) {
        pj_thread_register("keepAliveThread", desc, &pj_thread);
    }

    while (true) {
        this_thread::sleep_for(chrono::seconds(5));
        try {
            acc->setRegistration(true);
            cout << "[INFO] Re-registered to keep the account alive." << endl;
        }
        catch (const Error& err) {
            cerr << "[ERROR] Error during re-registration: " << err.info() << endl;
        }
    }
}

// Signal handler for graceful shutdown
bool isRunning = true;
void signalHandler(int signum) {
    cout << "[INFO] Signal received. Shutting down..." << endl;
    isRunning = false;
}

int main() {
    // Register signal handler
    signal(SIGINT, signalHandler);

    // Step 1: Initialize PJSUA
    EpConfig ep_cfg;
    Endpoint ep;

    try {
        ep.libCreate();
        ep_cfg.logConfig.level = 6; // Adjust log level
        ep.libInit(ep_cfg);

        TransportConfig tcfg;
        tcfg.port = 5060;
        ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
        ep.libStart();
        cout << "[INFO] PJSIP started and ready." << endl;

        // Step 2: Set up the Account
        string idUri = "sip:joe@demo.dial-afrika.com";
        string registrarUri = "sip:demo.dial-afrika.com";
        string username = "joe";
        string password = "Temp@123";

        MyAccount* acc = setupAccount(idUri, registrarUri, username, password);

        // Step 3: Start Keep-Alive Thread
        thread keepAliveThread(keepAlive, acc);

        // Step 4: Make an Outgoing Call
        string destination;
        cout << "Enter the SIP URI of the recipient (e.g., sip:username@sipdomain): ";
        cin >> destination;

        MyCall* call = new MyCall(*acc);
        CallOpParam call_prm;
        call_prm.opt.audioCount = 1;
        call_prm.opt.videoCount = 0;

        try {
            call->makeCall(destination, call_prm);
            cout << "[INFO] Outgoing call to " << destination << " initiated." << endl;
        }
        catch (const Error& err) {
            cerr << "[ERROR] Failed to make call: " << err.info() << endl;
        }

        // Step 5: Wait for User to Exit
        cout << "Press Ctrl+C to exit..." << endl;
        while (isRunning) {
            this_thread::sleep_for(chrono::seconds(1));
        }

        // Cleanup
        keepAliveThread.detach();
        delete acc;
        ep.libDestroy();
        cout << "[INFO] PJSIP shutdown complete." << endl;
    }
    catch (const Error& err) {
        cerr << "[ERROR] PJSIP error: " << err.info() << endl;
        return 1;
    }

    return 0;
}
