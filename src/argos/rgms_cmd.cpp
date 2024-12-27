////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2023 Matthew Deutsch
//
// Argos is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Argos is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Argos; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "fmt/bundled/color.h"
#include "zmq.hpp"
#include "zmq_addon.hpp"

#include "argos/main.h"
#include "util/arg.h"
#include "util/serial.h"
#include "util/clock.h"
#include "util/file.h"
#include "util/string.h"
#include "rgmui/rgmuimain.h"
#include "smb/rgms.h"

using namespace argos;
using namespace argos::util;
using namespace argos::main;

struct Lister {
    std::string Item;
    std::string Flavor;
    std::function<void(int argc, char** argv, std::ostream& os)> List;
};

static std::vector<Lister> InitListers()
{
    std::vector<Lister> listers;
    auto Put = [&](const char* i, const char* f, std::function<void(int argc, char** argv, std::ostream& os)> func){
	Lister l;
	l.Item = std::string(i);
	l.Flavor = std::string(f);
	l.List = func;
	listers.push_back(l);
    };
    Put("v4l2", "the v4l2 video sources (v4l2-ctl --list-devices)",
        [&](int, char**, std::ostream& os){
            int r = system("v4l2-ctl --list-devices");
        });
    Put("mwcap", "the magewell capture sources (mwcap-info -l)",
        [&](int, char**, std::ostream& os){
            int r = system("mwcap-info -l");
        });
    Put("pactl", "the pulse audio sources (pactl list short sources)",
        [&](int, char**, std::ostream& os){
            int r = system("pactl list short sources");
        });
    Put("serial", "the serial sources (sudo dmesg | grep tty)",
        [&](int, char**, std::ostream& os){
            int r = system("journalctl --dmesg -o short-monotonic --no-hostname --no-pager | grep tty");
            //os << "RUN this yourself:" << std::endl;
            //os << "sudo dmesg | grep tty" << std::endl;
        });

    //listers.emplace_back(std::string("v4l2"), std::string("the v4l2 video sources (v4l2-ctl --list-devices)"),
    //    [&](int, char**, std::ostream& os){
    //        int r = system("v4l2-ctl --list-devices");
    //    });
    //listers.emplace_back(std::string("mwcap"), std::string("the magewell capture sources (mwcap-info -l)"),
    //    [&](int, char**, std::ostream& os){
    //        int r = system("mwcap-info -l");
    //    });
    //listers.emplace_back(std::string("pactl"), std::string("the pulse audio sources (pactl list short sources)"),
    //    [&](int, char**, std::ostream& os){
    //        int r = system("pactl list short sources");
    //    });
    //listers.emplace_back(std::string("serial"), std::string("the serial sources (sudo dmesg | grep tty)"),
    //    [&](int, char**, std::ostream& os){
    //        os << "RUN this yourself:" << std::endl;
    //        os << "sudo dmesg | grep tty" << std::endl;
    //    });
    return listers;
}

static int DoList(int argc, char** argv)
{
    static std::vector<Lister> LISTERS = InitListers();
    std::string item;
    if (util::ArgReadString(&argc, &argv, &item)) {
        for (auto & lister : LISTERS) {
            if (lister.Item == item) {
                lister.List(argc, argv, std::cout);
                return 0;
            }
        }
    }

    int r = 0;
    std::ostream* o = &std::cout;
    if (item != "") {
        Error("ERROR: unknown item type supplied to 'list {}'",  item);
        o = &std::cerr;
        r = 1;
    }

    (*o) << "possible item types are:" << std::endl;
    for (auto & lister : LISTERS) {
        (*o) << "  '" << lister.Item << "'  " << lister.Flavor << std::endl;
    }
    return r;
}

static void DoSerialWatchConsole(std::string path, int baud) {
    if (path == "") {
        int r = system("journalctl --dmesg -o short-monotonic --no-hostname --no-pager | grep tty");
        std::cout << "enter tty path as /dev/ttyUSBx: " << std::endl;
        std::cin >> path;
    }

    std::unique_ptr<util::SimpleSerialPort> port;
    try {
        port = std::make_unique<util::SimpleSerialPort>(path, baud);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return;
    }
    std::cout << "open: " << path << " " << baud << std::endl;

    int READ_BUFFER_SIZE = 256;
    std::vector<uint8_t> readBuffer(READ_BUFFER_SIZE);

    auto info = internesceptor::MessageParseInfo::InitialState();
    int l = 0;
    for (;;) {
        int read = port->Read(readBuffer.data(), readBuffer.size());
        for (int i = 0; i < read; i++) {
            auto byte = readBuffer[i];
            auto status = internesceptor::ProgressMessageParse(&info, byte);

            fmt::color color;
            if (status == internesceptor::MessageParseStatus::WARNING_BYTE_IGNORED_WAITING) {
                color = fmt::color::yellow;
            } else if (status == internesceptor::MessageParseStatus::SUCCESS) {
                color = fmt::color::green;
            } else if (status == internesceptor::MessageParseStatus::AGAIN) {
                color = fmt::color::dark_gray;
            } else {
                color = fmt::color::white;
            }


            std::cout << fmt::format(fmt::fg(color), "{:02x}", byte) << std::flush;

            l++;
            if (l % 2 == 0) {
                fmt::print(" ");
            }
            if (l == 40) {
                fmt::print("\n");
                l = 0;
            }

            //if (status == internesceptor::MessageParseStatus::WARNING_BYTE_IGNORED_WAITING) {
            //    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
            //} else if (status == internesceptor::MessageParseStatus::SUCCESS) {
            //    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            //} else if (status == internesceptor::MessageParseStatus::AGAIN) {
            //    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 180, 180, 255));
            //} else {
            //    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            //}

        }
        if (g_SIGINT) {
            fmt::print("\n");
            fmt::print("Interrupted\n");
            break;
        }
    }
}

static int DoWatch(int argc, char** argv, const argos::RuntimeConfig* config)
{
    int r = 0;
    std::string item;
    if (util::ArgReadString(&argc, &argv, &item)) {
        if (item == "serial") {

            std::string ttyPath;
            int baud = 4000000;
            bool console = true;

            std::string arg;
            while (util::ArgReadString(&argc, &argv, &arg)) {
                if (arg == "--tty") {
                    if (!util::ArgReadString(&argc, &argv, &ttyPath)) {
                        Error("argument required to --tty");
                        return 1;
                    }
                } else if (arg == "--console") {
                    console = true;
                } else {
                    Error("Unknown argument to watch");
                }
            }

            if (console) {
                DoSerialWatchConsole(ttyPath, baud);
            } else {
                // TODO
                //WatchSerialApp app(config);
                //if (ttyPath != "") {
                //    app.OpenTTY(ttyPath, baud);
                //}
                //DoApp(info, "Watch Serial", &app, 1920, 1280);
            }
//        } else if (item == "v4l2") {
//            WatchSourceApp app;
//            std::string path;
//            if (util::ArgReadString(&argc, &argv, &path)) {
//                if (util::StringStartsWith(path, "/dev/video")) {
//                    app.Open(path, true);
//                } else {
//                    try {
//                        int i = std::stoi(path);
//                        app.Open(fmt::format("/dev/video{}", i), true);
//                    } catch (std::exception& e) {
//                        std::cerr << e.what() << std::endl;
//                        return 1;
//                    }
//                }
//            }
//            DoApp(info, "Watch v4l2", &app, 1920, 1280);
//        } else if (item == "pactl") {
//
//        } else if (item == "live") {
//            WatchSourceApp app;
//            std::string path;
//            if (util::ArgReadString(&argc, &argv, &path)) {
//                app.Open(path, true);
//            }
//            DoApp(info, "Watch Live", &app, 1920, 1280);
//        } else if (item == "video") {
//            WatchSourceApp app;
//            std::string path;
//            if (util::ArgReadString(&argc, &argv, &path)) {
//                app.Open(path, false);
//            }
//            DoApp(info, "Watch Video", &app, 1920, 1280);
        } else {
            std::cerr << "ERROR: unknown item type supplied to 'watch " << item << "'" << std::endl;
            r = 1;
        }
    } else {
        std::cerr << "ERROR: no item type supplied to 'watch'" << std::endl;
        r = 1;
    }

    if (r == 1) {
        std::cerr << "possible item types are:" << std::endl;
        std::cout << "  'serial' watching the internesceptor serial port" << std::endl;
        //std::cout << "  'v4l2' watching a v4l2 video source, via CVVideoCaptureSource (supports single int arg)" << std::endl;
        //std::cout << "  'live' watching a live video source, via CVVideoCaptureSource" << std::endl;
        //std::cout << "  'video' watching a static video file source, via CVVideoCaptureSource" << std::endl;
        //std::cout << "  'pactl' watching a pulse audio source" << std::endl;
    }
    return r;
}

static int DoTransmitStuff(const std::string& ttypath, const std::string& target, const std::string& name, const argos::RuntimeConfig* config, bool norecord) {
    smb::SMBDatabase db(config->ArgosPathTo("smb.db"));
    auto nametables = db.GetNametableCache();

    util::fs::create_directories(fmt::format("{}rec/", config->ArgosDirectory));
    std::string recordingPath = fmt::format("{}rec/{}_{}.rec", config->ArgosDirectory,
            util::GetTimestampNow(), name);

    argos::rgms::SMBSerialProcessorThreadInfo tinfo;
    argos::rgms::SMBSerialProcessorThread thread(ttypath, nametables);
    if (!norecord) {
        thread.StartRecording(recordingPath);
    }

    zmq::context_t context(2);
    zmq::socket_t socket(context, zmq::socket_type::pub);
    socket.bind(target);

    std::vector<uint8_t> buffer;

    int sleeps = 0;
    int totsent = 0;
    for (;;) {
        while (auto p = thread.GetNextProcessorOutput()) {
            if (p->Frame.NTDiffs.size() > 5000) {
                p->Frame.NTDiffs.resize(5000);
            }
            OutputToBytes(p, &buffer);
            socket.send(zmq::str_buffer("smb"), zmq::send_flags::sndmore);
            socket.send(zmq::message_t(name.data(), name.size()), zmq::send_flags::sndmore);
            socket.send(zmq::message_t(buffer.data(), buffer.size()), zmq::send_flags::none);
            totsent++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        sleeps++;
        if (sleeps == 250) {
            thread.GetInfo(&tinfo);
            fmt::print("bytes: {:10s} bps: {:8.1f} msgs: {:10d} mps: {:8.1f} err: {:5d} tot: {:6d}\n",

                    util::BytesFmt(tinfo.ByteCount), tinfo.ApproxBytesPerSecond,
                    tinfo.MessageCount, tinfo.ApproxMessagesPerSecond, tinfo.ErrorCount, totsent);

            sleeps = 0;
        }
        if (g_SIGINT) {
            fmt::print("\n");
            fmt::print("Interrupted\n");
            break;
        }
    }

    return 0;
}

static int DoTransmit(int argc, char** argv, argos::RuntimeConfig* config)
{
    if (argc < 3 || argc > 4) {
        Error("transmit <tty> <target> <name>");
        Error("transmit /dev/ttyUSB1 tcp://0.0.0.0:5555 seat1");
        return 1;
    }
    std::string ttypath(argv[0]);
    std::string target(argv[1]);
    std::string name(argv[2]);

    return DoTransmitStuff(ttypath, target, name, config, argc == 4);
}

static int DoReceiveStuff(const std::vector<std::string>& bindings)
{
    //socket.connect("tcp://192.168.0.3:5555");

    zmq::context_t context(2);
    zmq::socket_t socket(context, zmq::socket_type::sub);
    for (auto & bind : bindings) {
        socket.connect(bind);
    }
    socket.set(zmq::sockopt::subscribe, "smb");
    while(true) {
        std::vector<zmq::message_t> recv_msgs;
        zmq::recv_result_t result =
            zmq::recv_multipart(socket, std::back_inserter(recv_msgs));
        std::cout << recv_msgs[0].to_string() << " " << recv_msgs[1].to_string() << " " << recv_msgs[2].size() << std::endl;
        if (g_SIGINT) {
            fmt::print("\n");
            fmt::print("Interrupted\n");
            break;
        }
    }
    return 0;
}

static int DoReceive(int argc, char** argv)
{
    std::vector<std::string> bindings;
    if (argc == 0) {
        return 1;
    }
    for (int i = 0; i < argc; i++) {
        bindings.emplace_back(argv[i]);
    }
    return DoReceiveStuff(bindings);
}

static int DoSMBComp(int argc, char** argv, argos::RuntimeConfig* config)
{
    void* sharedMem = nullptr;

    std::string name;

    //int f = util::DoArgsFlags_PrintErrors | util::DoArgsFlags_ThrowErrors;
    int dno = -1, pdno = -1;

    std::string arg;
    while (util::ArgReadString(&argc, &argv, &arg)) {
        if (arg == "--name") {
            if (!util::ArgReadString(&argc, &argv, &name)) {
                Error("argument required to --name");
                return 1;
            }
        } else if (arg == "--aux-display") {
            if (!util::ArgReadInt(&argc, &argv, &dno)) {
                Error("argument required to --aux-display");
                return 1;
            }
        } else if (arg == "--prime-display") {
            if (!util::ArgReadInt(&argc, &argv, &pdno)) {
                Error("argument required to --prime-display");
                return 1;
            }
        } else {
            Error("Unknown argument to smbcomp");
        }
    }

    if (dno != -1) {
        sharedMem = mmap(NULL, SHARED_MEM_SIZE, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        uint8_t* b = reinterpret_cast<uint8_t*>(sharedMem);
        b[0] = 0x00;
        b[SHARED_MEM_QUIT] = 0x00;

        int pid = fork();
        if (pid == 0) {
            argos::rgms::SMBCompAppAux app2(sharedMem);

            rgmui::Window window("RGMS SMB Comp 2", 1920, 1080, -1, -1, dno, nullptr, nullptr, SDL_WINDOW_BORDERLESS);
            rgmui::WindowAppMainLoop(&window, &app2, std::chrono::microseconds(15000));
        } else {
            argos::rgms::SMBCompApp app(config);
            if (name != "") {
                app.LoadNamedConfig(name);
            }

            app.SetSharedMemory(sharedMem);
            RunIApplication(config, "RGMS SMB Comp", &app);
        }
    } else {
        argos::rgms::SMBCompApp app(config);
        if (name != "") {
            app.LoadNamedConfig(name);
        }
        RunIApplication(config, "RGMS SMB Comp", &app); //, 2400, 1180, pdno);
        //DoApp(config, "RGMS SMB Comp", &app, 2400, 1380);
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// The 'rgms' command is for bad original code that needs to be moved eventually
REGISTER_COMMAND(rgms, "RGMS",
R"(
EXAMPLES:

USAGE:

DESCRIPTION:

OPTIONS:

)")
{
    std::string action;
    if (!ArgReadString(&argc, &argv, &action)) {
        Error("at least one argument expected to 'rgms'");
        return 1;
    }

    if (action == "list") {
        return DoList(argc, argv);
    } else if (action == "watch") {
        return DoWatch(argc, argv, config);
    } else if (action == "transmit") {
        return DoTransmit(argc, argv, config);
    } else if (action == "receive") {
        return DoReceive(argc, argv);
    } else if (action == "smbcomp") {
        return DoSMBComp(argc, argv, config);
    } else if (action == "recreview") {

    } else {
        Error("unknown action. '{}' expected 'list', 'watch', 'transmit', 'receive', 'smbcomp', or 'recreview'", action);
        return 1;
    }

    return 0;
}
