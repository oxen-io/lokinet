
#include <QApplication>
#include <QMessageBox>
#include <CoreFoundation/CoreFoundation.h>

int uninstall();

int main(int argc, char * argv[])
{
  QApplication app{argc, argv};
  if(QMessageBox::question(nullptr, "Lokinet Uninstaller", "Do You want to uninstall Lokinet?",
                        QMessageBox::Yes|QMessageBox::No)
     == QMessageBox::Yes)
  {
    QMessageBox msgBox;
    const auto  retcode = uninstall();
    if(retcode == 0)
    {
      msgBox.setText("Lokinet has been successfully uninstalled, you may now remove the uninstaller if you wish.");
    }
    else
    {
      msgBox.setText("Failed to uninstall lokinet");
    }
    msgBox.exec();
  }
  return 0;
}

int uninstall()
{
  AuthorizationRef authorizationRef;
  OSStatus status;
  
  status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authorizationRef);
  
  char* tool = "/bin/bash";
  char* args[] = { "-c" , "/opt/lokinet/bin/lokinet_uninstall.sh", nullptr};
  FILE* pipe = NULL;
  
  return AuthorizationExecuteWithPrivileges(authorizationRef, tool, kAuthorizationFlagDefaults, args, &pipe);
}

