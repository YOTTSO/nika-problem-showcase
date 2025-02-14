/*
 * This source file is part of an OSTIS project. For the latest info, see http://ostis.net
 * Distributed under the MIT License
 * (See accompanying file COPYING.MIT or copy at http://opensource.org/licenses/MIT)
 */

#include <iostream>
#include <atomic>
#include <thread>

#include <sc-memory/sc_debug.hpp>
#include <sc-memory/utils/sc_signal_handler.hpp>

#include "sc-config/sc_config.hpp"
#include "sc_memory_config.hpp"

#include "sc_server_factory.hpp"

void PrintStartMessage()
{
  std::cout << "SC-SERVER USAGE\n\n"
            << "--config|-c -- Path to configuration file\n"
            << "--host|-h -- Sc-server host name, ip-address\n"
            << "--port|-p -- Sc-server port\n"
            << "--extensions_path|-e -- Path to directory with sc-memory extensions\n"
            << "--repo_path|-r -- Path to kb.bin folder\n"
            << "--verbose|-v -- Flag to don't save sc-memory state on exit\n"
            << "--clear -- Flag to clear sc-memory on start\n"
            << "--help -- Display this message\n"
            << "--test|-t -- Flag to test sc-server, run and stop it\n\n";
}

sc_bool RunServer(ScParams const & serverParams, ScMemoryConfig & memoryConfig, std::shared_ptr<ScServer> & server)
{
  sc_bool status = SC_FALSE;
  try
  {
    server = ScServerFactory::ConfigureScServer(serverParams, memoryConfig.GetParams());
    ScServerLogger * logger = ScServerFactory::ConfigureScServerLogger(server, serverParams);
    server->Run();
    server->ResetLogger(logger);

    status = SC_TRUE;
  }
  catch (utils::ScException const & e)
  {
    server->ResetLogger();
    server->LogMessage(ScServerErrorLevel::error, e.Message());
  }
  catch (std::exception const & e)
  {
    server->ResetLogger();
    server->LogMessage(ScServerErrorLevel::error, e.what());
  }

  return status;
}

sc_bool StopServer(std::shared_ptr<ScServer> const & server)
{
  sc_bool status = SC_FALSE;
  try
  {
    server->ResetLogger();
    server->Stop();
    status = SC_TRUE;
  }
  catch (std::exception const & e)
  {
    server->LogMessage(ScServerErrorLevel::error, e.what());
  }
  return status;
}

sc_int main(sc_int argc, sc_char * argv[])
try
{
  ScOptions options{argc, argv};

  if (options.Has({"help"}))
  {
    PrintStartMessage();
    return EXIT_SUCCESS;
  }

  std::string configFile;
  if (options.Has({"config", "c"}))
    configFile = options[{"config", "c"}].second;

  ScParams serverParams{options, {{"host", "h"}, {"port", "p"}}};
  ScConfig config{configFile, {"repo_path", "extensions_path", "log_file"}};
  auto serverConfig = config["sc-server"];
  for (auto const & key : *serverConfig)
    serverParams.insert({key, serverConfig[key]});

  ScParams memoryParams{options, {{"extensions_path", "e"}, {"repo_path", "r"}, {"verbose", "v"}, {"clear"}}};
  ScMemoryConfig memoryConfig{config, memoryParams};

  utils::ScSignalHandler::Initialize();

  std::shared_ptr<ScServer> server;
  sc_bool const status = RunServer(serverParams, memoryConfig, server);
  if (status == SC_FALSE)
  {
    StopServer(server);
    return EXIT_FAILURE;
  }

  std::atomic_bool isRun = {!options.Has({"test", "t"})};
  utils::ScSignalHandler::m_onTerminate = [&isRun]() {
    isRun = SC_FALSE;
  };

  while (isRun)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  return StopServer(server) ? EXIT_SUCCESS : EXIT_FAILURE;
}
catch (utils::ScException const & e)
{
  SC_LOG_ERROR(e.Description());
}
