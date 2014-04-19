#include <vector>
#include <string>
#include <sstream>
using namespace std;


#include <global.h>
#include <names.h>
#include <idtable.h>
#include <osmem.h>
#include <kfm.h>
#include <nsfdb.h>
#include <nsfnote.h>
#include <osmisc.h>
#include <nif.h>
#include <osenv.h>


#define CMD_ID "ID:" 
#define CMD_PASS "PASS:"
#define CMD_DB "DB:"
#define CMD_FOLDER "FOLDER:"
#define CMD_DOC "DOC:"
#define CMD_READ "READ:"
#define CMD_UNREAD "UNREAD:"

enum ACTION{
	ID,
	PASS,
	DB,
	FOLDER,
	DOC,
	READ,
	UNREAD,
	UNKNOWN
};

#define BUF_LEN		512
static	char		buf[BUF_LEN];
static	char		UserName[MAXUSERNAME+1];
static	int			UserNameLen = 0;
static	DBHANDLE	hDb;

#define SCAN_DATA(X) if (0 == _strnicmp(inbuf, CMD_ ## X, strlen(CMD_ ## X))) \
{ \
	action = X; \
	data = inbuf + strlen(CMD_ ## X); \
}

void __cdecl LoadAPIError(STATUS api_error, char *error_buf)
{
	strncpy_s(error_buf, BUF_LEN, "ERROR:", 6);
	OSLoadString(NULLHANDLE, ERR(api_error), error_buf + 6, BUF_LEN - 6);
}

STATUS __cdecl CountUnread(char* folderName, char* outbuf)
{
	STATUS         status = NOERROR;
	STATUS         stat2 = NOERROR;
	HANDLE         hTable;
	HANDLE         hFolderTable;
	DWORD          noteID = 0L;
	BOOL           scanFlag;
	NOTEID         ViewID;

	if (UserNameLen == 0)
	{
		if (status = SECKFMGetUserName(UserName))
			return status;
		UserNameLen = strlen(UserName);
	}

	/* Get the unread list */
	if (status = NSFDbGetUnreadNoteTable(hDb, UserName, UserNameLen, TRUE, &hTable))
		return (status);

	if(status = NSFDbUpdateUnread(hDb, hTable))
	{
		IDDestroyTable(hTable);
		return status;
	}

	if (status = NIFFindView(hDb, folderName, &ViewID))
	{
		IDDestroyTable(hTable);
		return status;
	}

	if (status = NSFFolderGetIDTable(hDb, hDb, ViewID, 0, &hFolderTable))
	{
		IDDestroyTable(hTable);
		return status;
	}

	scanFlag = TRUE;
	//printf("Unread notes:\n");
	int count = 0;
	while (IDScan(hTable, scanFlag, &noteID))
	{
		scanFlag = FALSE;
		if (!(RRV_DELETED & noteID) && IDIsPresent(hFolderTable, noteID)){
			count++;
			//printf("%lX ", noteID);
		}
	}
	sprintf_s(outbuf, BUF_LEN, "OK %d\n", count);

	status = IDDestroyTable(hFolderTable);
	stat2 = IDDestroyTable(hTable);
	if (NOERROR != status)
		return (status);
	else
		return (stat2);
}

void __cdecl process(char* inbuf, char* outbuf)
{
	
	char tmp[BUF_LEN];
	char* data = NULL;
	ACTION action = UNKNOWN;
	STATUS      status = 0;
	
	ZeroMemory(outbuf, BUF_LEN);
	int len = strlen(inbuf);
	if (inbuf[len - 1] == '\n')	inbuf[len - 1] = 0;
	if (inbuf[len - 2] == '\r')	inbuf[len - 2] = 0;
	SCAN_DATA(ID) else SCAN_DATA(PASS) else SCAN_DATA(DB) else SCAN_DATA(FOLDER) else SCAN_DATA(READ) else SCAN_DATA(UNREAD);

	switch (action)
	{
	case ID:
		strcpy_s(buf, BUF_LEN, data);
		if (status = NotesInitExtended(0, NULL))
		{
			strcpy_s(outbuf, BUF_LEN, "ERROR: Unable to initialize Notes.\n");
			return;
		}
		strcpy_s(outbuf, BUF_LEN, "OK\n");
		return;
	case PASS:
		strcpy_s(tmp, BUF_LEN, data);
		if (status = SECKFMSwitchToIDFile(buf, tmp, UserName, MAXUSERNAME + 1, 0, NULL)){
			LoadAPIError(status, outbuf);
			return;
		}
		UserNameLen = strlen(UserName);
		strcpy_s(outbuf, BUF_LEN, "OK\n");
		return;
	case DB:
		strcpy_s(buf, BUF_LEN, data);
		if(status = NSFDbOpen(buf, &hDb))
		{
			LoadAPIError(status, outbuf);
			return;
		}
		strcpy_s(outbuf, BUF_LEN, "OK\n");
		return;
	case FOLDER:
		strcpy_s(buf, BUF_LEN, data);
		if (status = CountUnread(buf, outbuf)){
			LoadAPIError(status, outbuf);
			return;
		}
		return;
	default:
		strcpy_s(outbuf, BUF_LEN, "ERROR: Command unknown\n");
		return;
	}
	
}