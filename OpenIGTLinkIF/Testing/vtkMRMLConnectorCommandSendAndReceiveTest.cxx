#include "vtkSlicerConfigure.h"

//OpenIGTLink includes
#include "igtlOSUtil.h"

// IF module includes
#include "vtkMRMLIGTLConnectorNode.h"

// VTK includes
#include <vtkTimerLog.h>
#include <vtksys/SystemTools.hxx>
#include <vtkSmartPointer.h>
#include <vtkObject.h>
#include <vtkObjectFactory.h>
#include <vtkWeakPointer.h>
#include <vtkCallbackCommand.h>
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkTestingOutputWindow.h"

class CommandObserver:public vtkObject
{
public:
  static CommandObserver *New(){
    VTK_STANDARD_NEW_BODY(CommandObserver);
  };
  vtkTypeMacro(CommandObserver, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE
  {
  vtkObject::PrintSelf(os, indent);
  };

  ~CommandObserver(){};

  void onCommandReceivedEventFunc(vtkObject* caller, unsigned long eid, void *calldata)
  {
  igtlioCommand* command = reinterpret_cast<igtlioCommand*>(calldata);
  command->SetResponseContent(ResponseString.c_str());
  ConnectorNode->SendCommandResponse(command);
  std::cout << "*** COMMAND received from client:" << std::endl;
  std::cout << command->GetCommandContent() << std::endl;
  testSuccessful +=1;
  }

  void onCommandResponseReceivedEventFunc(vtkObject* caller, unsigned long eid,  void *calldata)
  {
  std::cout << "*** COMMAND response received from server:" << std::endl;
  igtlioCommand* command = reinterpret_cast<igtlioCommand*>(calldata);
  std::cout << command->GetResponseContent() << std::endl;
  if (ResponseString.compare(command->GetResponseContent()) == 0)
    {
    testSuccessful +=1;
    }
  }
  int testSuccessful;
  std::string ResponseString;
  vtkMRMLIGTLConnectorNode* ConnectorNode;

protected:
  CommandObserver()
  {
  testSuccessful = 0;
  ConnectorNode = NULL;
  ResponseString = "<Command>\n <Result success=\"true\"> <Parameter Name=\"Depth\" /> </Result>\n</Command>";
  };

};

int vtkMRMLConnectorCommandSendAndReceiveTest(int argc, char * argv [] )
{
  int port = 18955;

  // Setup the Server and client, as well as the event observers.
  vtkSmartPointer<CommandObserver> commandServerObsever = vtkSmartPointer<CommandObserver>::New();
  vtkSmartPointer<vtkMRMLIGTLConnectorNode> serverConnectorNode = vtkMRMLIGTLConnectorNode::New();
  commandServerObsever->ConnectorNode = serverConnectorNode.GetPointer();
  serverConnectorNode->AddObserver(serverConnectorNode->CommandReceivedEvent, commandServerObsever, &CommandObserver::onCommandReceivedEventFunc);
  // The connector type, server port, and etc,  are set by the qSlicerIGTLConnectorPropertyWidget
  // To make the test simple, just set the port directly.
  serverConnectorNode->SetTypeServer(port);
  serverConnectorNode->Start();
  igtl::Sleep(20);
  vtkSmartPointer<vtkMRMLIGTLConnectorNode> clientConnectorNode = vtkMRMLIGTLConnectorNode::New();
  clientConnectorNode->AddObserver(clientConnectorNode->CommandResponseReceivedEvent, commandServerObsever, &CommandObserver::onCommandResponseReceivedEventFunc);
  clientConnectorNode->SetTypeClient("localhost", port);
  clientConnectorNode->Start();

  // Make sure the server and client are connected.
  double timeout = 5;
  double starttime = vtkTimerLog::GetUniversalTime();

  // Client connects to server.
  while (vtkTimerLog::GetUniversalTime() - starttime < timeout)
    {
    serverConnectorNode->PeriodicProcess();
    clientConnectorNode->PeriodicProcess();
    vtksys::SystemTools::Delay(5);

    if (clientConnectorNode->GetState() == vtkMRMLIGTLConnectorNode::StateConnected)
      {
      std::cout << "SUCCESS: connected to server" << std::endl;
      break;
      }
    if (clientConnectorNode->GetState() == vtkMRMLIGTLConnectorNode::StateOff)
      {
      std::cout << "FAILURE to connect to server" << std::endl;
      clientConnectorNode->Stop();
      serverConnectorNode->Stop();
      clientConnectorNode->Delete();
      serverConnectorNode->Delete();
      return EXIT_FAILURE;
      }
    }

  clientConnectorNode->SendCommand("Get", "<Command>\n <Parameter Name=\"Depth\" />\n </Command>", false);

  // Make sure the Server receive the command message.
  starttime = vtkTimerLog::GetUniversalTime();
  while (vtkTimerLog::GetUniversalTime() - starttime < timeout)
    {
    serverConnectorNode->PeriodicProcess();
    vtksys::SystemTools::Delay(5);
    }

  // Make sure the Client receive the response message.
  starttime = vtkTimerLog::GetUniversalTime();
  while (vtkTimerLog::GetUniversalTime() - starttime < timeout)
    {
    clientConnectorNode->PeriodicProcess();
    vtksys::SystemTools::Delay(5);
    }
  clientConnectorNode->Stop();
  serverConnectorNode->Stop();
  clientConnectorNode->Delete();
  serverConnectorNode->Delete();

  //Condition only holds when both onCommandReceivedEventFunc and onCommandResponseReceivedEventFunc are called.
  CHECK_INT(commandServerObsever->testSuccessful, 2);
  return EXIT_SUCCESS;
}