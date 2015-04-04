// --------------------------------------------------------------------------
//
// File
//		Name:    Daemon.cpp
//		Purpose: Basic daemon functionality
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------

#include "Box.h"

#ifdef HAVE_UNISTD_H
	#include <unistd.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>

#ifdef WIN32
	#include <ws2tcpip.h>
#endif

#include <iostream>

#include "Daemon.h"
#include "Configuration.h"
#include "ServerException.h"
#include "Guards.h"
#include "UnixUser.h"
#include "FileModificationTime.h"
#include "Logging.h"
#include "Utils.h"

#include "MemLeakFindOn.h"

Daemon *Daemon::spDaemon = 0;


// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::Daemon()
//		Purpose: Constructor
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
Daemon::Daemon()
	: mReloadConfigWanted(false),
	  mTerminateWanted(false),
	#ifdef WIN32
	  mSingleProcess(true),
	  mRunInForeground(true),
	  mKeepConsoleOpenAfterFork(true),
	#else
	  mSingleProcess(false),
	  mRunInForeground(false),
	  mKeepConsoleOpenAfterFork(false),
	#endif
	  mHaveConfigFile(false),
	  mAppName(DaemonName())
{
	// In debug builds, switch on assert failure logging to syslog
	ASSERT_FAILS_TO_SYSLOG_ON
	// And trace goes to syslog too
	TRACE_TO_SYSLOG(true)
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::~Daemon()
//		Purpose: Destructor
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
Daemon::~Daemon()
{
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::GetOptionString()
//		Purpose: Returns the valid Getopt command-line options.
//			 This should be overridden by subclasses to add
//			 their own options, which should override 
//			 ProcessOption, handle their own, and delegate to
//			 ProcessOption for the standard options.
//		Created: 2007/09/18
//
// --------------------------------------------------------------------------
std::string Daemon::GetOptionString()
{
	return "c:"
	#ifndef WIN32
		"DFK"
	#endif
		"hkPqQt:TUvVW:";
}

void Daemon::Usage()
{
	std::cout << 
	DaemonBanner() << "\n"
	"\n"
	"Usage: " << mAppName << " [options] [config file]\n"
	"\n"
	"Options:\n"
	"  -c <file>  Use the specified configuration file. If -c is omitted, the last\n"
	"             argument is the configuration file, or else the default \n"
	"             [" << GetConfigFileName() << "]\n"
#ifndef WIN32
	"  -D         Debugging mode, do not fork, one process only, one client only\n"
	"  -F         Do not fork into background, but fork to serve multiple clients\n"
#endif
	"  -k         Keep console open after fork, keep writing log messages to it\n"
#ifndef WIN32
	"  -K         Stop writing log messages to console while daemon is running\n"
	"  -P         Show process ID (PID) in console output\n"
#endif
	"  -q         Run more quietly, reduce verbosity level by one, can repeat\n"
	"  -Q         Run at minimum verbosity, log nothing\n"
	"  -v         Run more verbosely, increase verbosity level by one, can repeat\n"
	"  -V         Run at maximum verbosity, log everything\n"
	"  -W <level> Set verbosity to error/warning/notice/info/trace/everything\n"
	"  -t <tag>   Tag console output with specified marker\n"
	"  -T         Timestamp console output\n"
	"  -U         Timestamp console output with microseconds\n";
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::ProcessOption(int option)
//		Purpose: Processes the supplied option (equivalent to the
//			 return code from getopt()). Return zero if the
//			 option was handled successfully, or nonzero to
//			 abort the program with that return value.
//		Created: 2007/09/18
//
// --------------------------------------------------------------------------
int Daemon::ProcessOption(signed int option)
{
	switch(option)
	{
		case 'c':
		{
			mConfigFileName = optarg;
			mHaveConfigFile = true;
		}
		break;

#ifndef WIN32
		case 'D':
		{
			mSingleProcess = true;
		}
		break;

		case 'F':
		{
			mRunInForeground = true;
		}
		break;
#endif // !WIN32

		case 'k':
		{
			mKeepConsoleOpenAfterFork = true;
		}
		break;

		case 'K':
		{
			mKeepConsoleOpenAfterFork = false;
		}
		break;

		case 'h':
		{
			Usage();
			return 2;
		}
		break;

		case 'P':
		{
			Console::SetShowPID(true);
		}
		break;

		case 'q':
		{
			if(mLogLevel == Log::NOTHING)
			{
				BOX_FATAL("Too many '-q': "
					"Cannot reduce logging "
					"level any more");
				return 2;
			}
			mLogLevel--;
		}
		break;

		case 'Q':
		{
			mLogLevel = Log::NOTHING;
		}
		break;


		case 'v':
		{
			if(mLogLevel == Log::EVERYTHING)
			{
				BOX_FATAL("Too many '-v': "
					"Cannot increase logging "
					"level any more");
				return 2;
			}
			mLogLevel++;
		}
		break;

		case 'V':
		{
			mLogLevel = Log::EVERYTHING;
		}
		break;

		case 'W':
		{
			mLogLevel = Logging::GetNamedLevel(optarg);
			if (mLogLevel == Log::INVALID)
			{
				BOX_FATAL("Invalid logging level");
				return 2;
			}
		}
		break;

		case 't':
		{
			Logging::SetProgramName(optarg);
			Console::SetShowTag(true);
		}
		break;

		case 'T':
		{
			Console::SetShowTime(true);
		}
		break;

		case 'U':
		{
			Console::SetShowTime(true);
			Console::SetShowTimeMicros(true);
		}
		break;

		case '?':
		{
			BOX_FATAL("Unknown option on command line: " 
				<< "'" << (char)optopt << "'");
			return 2;
		}
		break;

		default:
		{
			BOX_FATAL("Unknown error in getopt: returned "
				<< "'" << option << "'");
			return 1;
		}
	}

	return 0;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::Main(const char *, int, const char *[])
//		Purpose: Parses command-line options, and then calls
//			Main(std::string& configFile, bool singleProcess)
//			to start the daemon.
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
int Daemon::Main(const std::string& rDefaultConfigFile, int argc,
	const char *argv[])
{
	// Find filename of config file
	mConfigFileName = rDefaultConfigFile;
	mAppName = argv[0];

	#ifdef BOX_RELEASE_BUILD
	mLogLevel = Log::NOTICE; // need an int to do math with
	#else
	mLogLevel = Log::INFO; // need an int to do math with
	#endif

	if (argc == 2 && strcmp(argv[1], "/?") == 0)
	{
		Usage();
		return 2;
	}

	signed int c;

	// reset getopt, just in case anybody used it before.
	// unfortunately glibc and BSD differ on this point!
	// http://www.ussg.iu.edu/hypermail/linux/kernel/0305.3/0262.html
	#if HAVE_DECL_OPTRESET == 1 || defined WIN32
		optind = 1;
		optreset = 1;
	#elif defined __GLIBC__
		optind = 0;
	#else // Solaris, any others?
		optind = 1;
	#endif

	while((c = getopt(argc, (char * const *)argv, 
		GetOptionString().c_str())) != -1)
	{
		int returnCode = ProcessOption(c);

		if (returnCode != 0)
		{
			return returnCode;
		}
	}

	if (argc > optind && !mHaveConfigFile)
	{
		mConfigFileName = argv[optind]; optind++;
		mHaveConfigFile = true;
	}

	if (argc > optind && ::strcmp(argv[optind], "SINGLEPROCESS") == 0)
	{
		mSingleProcess = true; optind++;
	}

	if (argc > optind)
	{
		BOX_FATAL("Unknown parameter on command line: "
			<< "'" << std::string(argv[optind]) << "'");
		return 2;
	}

	Logging::FilterConsole((Log::Level)mLogLevel);
	Logging::FilterSyslog ((Log::Level)mLogLevel);

	return Main(mConfigFileName);
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::Configure(const std::string& rConfigFileName)
//		Purpose: Loads daemon configuration. Useful when you have
//			 a local Daemon object and don't intend to fork()
//			 or call Main().
//		Created: 2008/04/19
//
// --------------------------------------------------------------------------

bool Daemon::Configure(const std::string& rConfigFileName)
{
	// Load the configuration file.
	std::string errors;
	std::auto_ptr<Configuration> apConfig;

	try
	{
		if (!FileExists(rConfigFileName.c_str()))
		{
			BOX_FATAL("The main configuration file for " <<
				DaemonName() << " was not found: " <<
				rConfigFileName);
			if (!mHaveConfigFile)
			{
				BOX_WARNING("The default configuration "
					"directory has changed from /etc/box "
					"to /etc/boxbackup");
			}
			return false;
		}
			
		apConfig = Configuration::LoadAndVerify(rConfigFileName,
			GetConfigVerify(), errors);
	}
	catch(BoxException &e)
	{
		if(e.GetType() == CommonException::ExceptionType &&
			e.GetSubType() == CommonException::OSFileOpenError)
		{
			BOX_ERROR("Failed to open configuration file: "  <<
				rConfigFileName);
			return false;
		}

		throw;
	}

	// Got errors?
	if(apConfig.get() == 0)
	{
		BOX_ERROR("Failed to load or verify configuration file");
		return false;
	}
	
	if(!Configure(*apConfig))
	{
		BOX_ERROR("Failed to verify configuration file");
		return false;		
	}
	
	// Store configuration
	mConfigFileName = rConfigFileName;
	mLoadedConfigModifiedTime = GetConfigFileModifiedTime();
		
	return true;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::Configure(const Configuration& rConfig)
//		Purpose: Loads daemon configuration. Useful when you have
//			 a local Daemon object and don't intend to fork()
//			 or call Main().
//		Created: 2008/08/12
//
// --------------------------------------------------------------------------

bool Daemon::Configure(const Configuration& rConfig)
{
	std::string errors;

	// Verify() may modify the configuration, e.g. adding default values
	// for required keys, so need to make a copy here
	std::auto_ptr<Configuration> apConf(new Configuration(rConfig));
	apConf->Verify(*GetConfigVerify(), errors);

	// Got errors?
	if(!errors.empty())
	{
		BOX_ERROR("Configuration errors: " << errors);
		return false;
	}
	
	// Store configuration
	mapConfiguration = apConf;
	
	// Let the derived class have a go at setting up stuff
	// in the initial process
	SetupInInitialProcess();
		
	return true;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::Main(const std::string& rConfigFileName)
//		Purpose: Starts the daemon off -- equivalent of C main() function
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
int Daemon::Main(const std::string &rConfigFileName)
{
	// Banner (optional)
	{
		BOX_SYSLOG(Log::NOTICE, DaemonBanner());
	}

	std::string pidFileName;

	bool asDaemon = !mSingleProcess && !mRunInForeground;

	try
	{
		if (!Configure(rConfigFileName))
		{
			BOX_FATAL("Failed to start: failed to load "
				"configuration file: " << rConfigFileName);
			return 1;
		}
		
		// Server configuration
		const Configuration &serverConfig(
			mapConfiguration->GetSubConfiguration("Server"));

		if(serverConfig.KeyExists("LogFacility"))
		{
			std::string facility =
				serverConfig.GetKeyValue("LogFacility");
			Logging::SetFacility(Syslog::GetNamedFacility(facility));
		}

		// Open PID file for writing
		pidFileName = serverConfig.GetKeyValue("PidFile");
		FileHandleGuard<(O_WRONLY | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)> pidFile(pidFileName.c_str());
	
#ifndef WIN32
		// Handle changing to a different user
		if(serverConfig.KeyExists("User"))
		{
			// Config file specifies an user -- look up
			UnixUser daemonUser(serverConfig.GetKeyValue("User").c_str());
			
			// Change the owner on the PID file, so it can be deleted properly on termination
			if(::fchown(pidFile, daemonUser.GetUID(), daemonUser.GetGID()) != 0)
			{
				THROW_EXCEPTION(ServerException, CouldNotChangePIDFileOwner)
			}
			
			// Change the process ID
			daemonUser.ChangeProcessUser();
		}

		if(asDaemon)
		{
			// Let's go... Daemonise...
			switch(::fork())
			{
			case -1:
				// error
				THROW_EXCEPTION(ServerException, DaemoniseFailed)
				break;

			default:
				// parent
				// _exit(0);
				return 0;
				break;

			case 0:
				// child
				break;
			}

			// In child

			// Set new session
			if(::setsid() == -1)
			{
				BOX_LOG_SYS_ERROR("Failed to setsid()");
				THROW_EXCEPTION(ServerException, DaemoniseFailed)
			}

			// Fork again...
			switch(::fork())
			{
			case -1:
				// error
				BOX_LOG_SYS_ERROR("Failed to fork() a child");
				THROW_EXCEPTION(ServerException, DaemoniseFailed)
				break;

			default:
				// parent
				_exit(0);
				return 0;
				break;

			case 0:
				// child
				break;
			}
		}
#endif // !WIN32
		
		// Must set spDaemon before installing signal handler,
		// otherwise the handler will crash if invoked too soon.
		if(spDaemon != NULL)
		{
			THROW_EXCEPTION(ServerException, AlreadyDaemonConstructed)
		}
		spDaemon = this;
		
#ifndef WIN32
		// Set signal handler
		// Don't do this in the parent, since it might be anything
		// (e.g. test/bbackupd)
		
		struct sigaction sa;
		sa.sa_handler = SignalHandler;
		sa.sa_flags = 0;
		sigemptyset(&sa.sa_mask); // macro
		if(::sigaction(SIGHUP, &sa, NULL) != 0 ||
			::sigaction(SIGTERM, &sa, NULL) != 0)
		{
			BOX_LOG_SYS_ERROR("Failed to set signal handlers");
			THROW_EXCEPTION(ServerException, DaemoniseFailed)
		}
#endif // !WIN32

		// Write PID to file
		char pid[32];

		int pidsize = sprintf(pid, "%d", (int)getpid());

		if(::write(pidFile, pid, pidsize) != pidsize)
		{
			BOX_LOG_SYS_FATAL("Failed to write PID file: " <<
				pidFileName);
			THROW_EXCEPTION(ServerException, DaemoniseFailed)
		}
		
		// Set up memory leak reporting
		#ifdef BOX_MEMORY_LEAK_TESTING
		{
			char filename[256];
			sprintf(filename, "%s.memleaks", DaemonName());
			memleakfinder_setup_exit_report(filename, DaemonName());
		}
		#endif // BOX_MEMORY_LEAK_TESTING
	
		if(asDaemon && !mKeepConsoleOpenAfterFork)
		{
#ifndef WIN32
			// Close standard streams
			::close(0);
			::close(1);
			::close(2);
			
			// Open and redirect them into /dev/null
			int devnull = ::open(PLATFORM_DEV_NULL, O_RDWR, 0);
			if(devnull == -1)
			{
				BOX_LOG_SYS_ERROR("Failed to open /dev/null");
				THROW_EXCEPTION(CommonException, OSFileError);
			}
			// Then duplicate them to all three handles
			if(devnull != 0) dup2(devnull, 0);
			if(devnull != 1) dup2(devnull, 1);
			if(devnull != 2) dup2(devnull, 2);
			// Close the original handle if it was opened above the std* range
			if(devnull > 2)
			{
				::close(devnull);
			}

			// And definitely don't try and send anything to those file descriptors
			// -- this has in the past sent text to something which isn't expecting it.
			TRACE_TO_STDOUT(false);
#endif // ! WIN32
			Logging::ToConsole(false);
		}

		// Log the start message
		BOX_NOTICE("Starting daemon, version: " << BOX_VERSION);
		BOX_NOTICE("Using configuration file: " << mConfigFileName);
	}
	catch(BoxException &e)
	{
		BOX_FATAL("Failed to start: exception " << e.what() 
			<< " (" << e.GetType() 
			<< "/"  << e.GetSubType() << ")");
		return 1;
	}
	catch(std::exception &e)
	{
		BOX_FATAL("Failed to start: exception " << e.what());
		return 1;
	}
	catch(...)
	{
		BOX_FATAL("Failed to start: unknown error");
		return 1;
	}

#ifdef WIN32
	// Under win32 we must initialise the Winsock library
	// before using sockets

	WSADATA info;

	if (WSAStartup(0x0101, &info) == SOCKET_ERROR)
	{
		// will not run without sockets
		BOX_FATAL("Failed to initialise Windows Sockets");
		THROW_EXCEPTION(CommonException, Internal)
	}
#endif

	int retcode = 0;
	
	// Main Daemon running
	try
	{
		while(!mTerminateWanted)
		{
			Run();
			
			if(mReloadConfigWanted && !mTerminateWanted)
			{
				// Need to reload that config file...
				BOX_NOTICE("Reloading configuration file: "
					<< mConfigFileName);
				std::string errors;
				std::auto_ptr<Configuration> pconfig(
					Configuration::LoadAndVerify(
						mConfigFileName.c_str(),
						GetConfigVerify(), errors));

				// Got errors?
				if(pconfig.get() == 0 || !errors.empty())
				{
					// Tell user about errors
					BOX_FATAL("Error in configuration "
						<< "file: " << mConfigFileName
						<< ": " << errors);
					// And give up
					retcode = 1;
					break;
				}
				
				// Store configuration
				mapConfiguration = pconfig;
				mLoadedConfigModifiedTime =
					GetConfigFileModifiedTime();
				
				// Stop being marked for loading config again
				mReloadConfigWanted = false;
			}
		}
		
		// Delete the PID file
		::unlink(pidFileName.c_str());
		
		// Log
		BOX_NOTICE("Terminating daemon");
	}
	catch(BoxException &e)
	{
		BOX_FATAL("Terminating due to exception " << e.what() 
			<< " (" << e.GetType() 
			<< "/"  << e.GetSubType() << ")");
		retcode = 1;
	}
	catch(std::exception &e)
	{
		BOX_FATAL("Terminating due to exception " << e.what());
		retcode = 1;
	}
	catch(...)
	{
		BOX_FATAL("Terminating due to unknown exception");
		retcode = 1;
	}

#ifdef WIN32
	WSACleanup();
#else
	// Should clean up here, but it breaks memory leak tests.
	/*
	if(asDaemon)
	{
		// we are running in the child by now, and should not return
		mapConfiguration.reset();
		exit(0);
	}
	*/
#endif

	ASSERT(spDaemon == this);
	spDaemon = NULL;

	return retcode;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::EnterChild()
//		Purpose: Sets up for a child task of the main server. Call
//		just after fork().
//		Created: 2003/07/31
//
// --------------------------------------------------------------------------
void Daemon::EnterChild()
{
#ifndef WIN32
	// Unset signal handlers
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);			// macro
	::sigaction(SIGHUP, &sa, NULL);
	::sigaction(SIGTERM, &sa, NULL);
#endif
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::SignalHandler(int)
//		Purpose: Signal handler
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
void Daemon::SignalHandler(int sigraised)
{
#ifndef WIN32
	if(spDaemon != 0)
	{
		switch(sigraised)
		{
		case SIGHUP:
			spDaemon->mReloadConfigWanted = true;
			break;
			
		case SIGTERM:
			spDaemon->mTerminateWanted = true;
			break;
		
		default:
			break;
		}
	}
#endif
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::DaemonName() 
//		Purpose: Returns name of the daemon
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
const char *Daemon::DaemonName() const
{
	return "generic-daemon";
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::DaemonBanner()
//		Purpose: Returns the text banner for this daemon's startup
//		Created: 1/1/04
//
// --------------------------------------------------------------------------
std::string Daemon::DaemonBanner() const
{
	return "Generic daemon using the Box Application Framework";
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::Run()
//		Purpose: Main run function after basic Daemon initialisation
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
void Daemon::Run()
{
	while(!StopRun())
	{
		::sleep(10);
	}
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::GetConfigVerify()
//		Purpose: Returns the configuration file verification structure for this daemon
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
const ConfigurationVerify *Daemon::GetConfigVerify() const
{
	static ConfigurationVerifyKey verifyserverkeys[] = 
	{
		DAEMON_VERIFY_SERVER_KEYS
	};

	static ConfigurationVerify verifyserver[] = 
	{
		{
			"Server",
			0,
			verifyserverkeys,
			ConfigTest_Exists | ConfigTest_LastEntry,
			0
		}
	};

	static ConfigurationVerify verify =
	{
		"root",
		verifyserver,
		0,
		ConfigTest_Exists | ConfigTest_LastEntry,
		0
	};

	return &verify;
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::GetConfiguration()
//		Purpose: Returns the daemon configuration object
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
const Configuration &Daemon::GetConfiguration() const
{
	if(mapConfiguration.get() == 0)
	{
		// Shouldn't get anywhere near this if a configuration file can't be loaded
		THROW_EXCEPTION(ServerException, Internal)
	}
	
	return *mapConfiguration;
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::SetupInInitialProcess()
//		Purpose: A chance for the daemon to do something initial
//			 setting up in the process which initiates
//			 everything, and after the configuration file has
//			 been read and verified.
//		Created: 2003/08/20
//
// --------------------------------------------------------------------------
void Daemon::SetupInInitialProcess()
{
	// Base class doesn't do anything.
}


void Daemon::SetProcessTitle(const char *format, ...)
{
	// On OpenBSD, setproctitle() sets the process title to imagename: <text> (imagename)
	// -- make sure other platforms include the image name somewhere so ps listings give
	// useful information.

#ifdef HAVE_SETPROCTITLE
	// optional arguments
	va_list args;
	va_start(args, format);

	// Make the string
	char title[256];
	::vsnprintf(title, sizeof(title), format, args);
	
	// Set process title
	::setproctitle("%s", title);
	
#endif // HAVE_SETPROCTITLE
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::GetConfigFileModifiedTime()
//		Purpose: Returns the timestamp when the configuration file
//			 was last modified
//
//		Created: 2006/01/29
//
// --------------------------------------------------------------------------

box_time_t Daemon::GetConfigFileModifiedTime() const
{
	EMU_STRUCT_STAT st;

	if(EMU_STAT(GetConfigFileName().c_str(), &st) != 0)
	{
		if (errno == ENOENT)
		{
			return 0;
		}
		BOX_LOG_SYS_ERROR("Failed to stat configuration file: " <<
			GetConfigFileName());
		THROW_EXCEPTION(CommonException, OSFileError)
	}
	
	return FileModificationTime(st);
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    Daemon::GetLoadedConfigModifiedTime()
//		Purpose: Returns the timestamp when the configuration file
//			 had been last modified, at the time when it was 
//			 loaded
//
//		Created: 2006/01/29
//
// --------------------------------------------------------------------------

box_time_t Daemon::GetLoadedConfigModifiedTime() const
{
	return mLoadedConfigModifiedTime;
}

