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
static	char		id_file[BUF_LEN];
static	char		UserName[MAXUSERNAME+1];
static	int			UserNameLen = 0;
static	DBHANDLE	hDb;

#define SCAN_DATA(X) \
if (0 == _strnicmp(inbuf, CMD_ ## X, strlen(CMD_ ## X))) \
{ \
	action = X; \
	OSTranslate(OS_TRANSLATE_UTF8_TO_LMBCS, inbuf + strlen(CMD_ ## X), BUF_LEN - strlen(CMD_ ## X), buf, BUF_LEN); \
}

#define CHECK(X) \
	status = X; \
	if (status != NOERROR){ \
	\
	LoadAPIError(status, outbuf); \
	return; \
} 


void __cdecl LoadAPIError(STATUS api_error, char *error_buf)
{
	strncpy_s(error_buf, BUF_LEN, "ERROR:", 6);
	OSLoadString(NULLHANDLE, ERR(api_error), error_buf + 6, BUF_LEN - 6);
	int len = strlen(error_buf); 
	error_buf[len] = '\n'; 
	error_buf[len + 1] = 0;

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

STATUS __cdecl IsDocUnread(NOTEID noteID, char* outbuf){
	STATUS      status = 0;
	HANDLE         hTable;

	if (UserNameLen == 0)
	{
		if (status = SECKFMGetUserName(UserName))
			return status;
		UserNameLen = strlen(UserName);
	}

	if (status = NSFDbGetUnreadNoteTable(hDb, UserName, UserNameLen, TRUE, &hTable))
		return (status);
	if (status = NSFDbUpdateUnread(hDb, hTable))
	{
		IDDestroyTable(hTable);
		return status;
	}

	if (IDIsPresent(hTable, noteID)){
		sprintf_s(outbuf, BUF_LEN, "OK 1\n");
	}
	else {
		sprintf_s(outbuf, BUF_LEN, "OK 0\n");
	}
	return IDDestroyTable(hTable);
}

STATUS __cdecl MarkDoc(NOTEID noteID, bool markUnread)
{
	STATUS         status = NOERROR;
	STATUS         stat2 = NOERROR;
	HANDLE         hTable;
	HANDLE         hOriginalTable;
	NOTEHANDLE     hNote;
	//int            index;
	BOOL           gotUndoID = FALSE;

	if (UserNameLen == 0)
	{
		if (status = SECKFMGetUserName(UserName))
			return status;
		UserNameLen = strlen(UserName);
	}

	if (status = NSFDbGetUnreadNoteTable(hDb, UserName, UserNameLen, TRUE, &hTable))
		return (status);

	if (status = NSFDbUpdateUnread(hDb, hTable))
	{
		IDDestroyTable(hTable);
		return status;
	}

	status = IDTableCopy(hTable, &hOriginalTable);
	if (NOERROR != status)
	{
		IDDestroyTable(hTable);
		return (status);
	}

	if (markUnread)
	{
		/* Adding a Note ID */
		/* (Marks note as Unread) */
		if (IDIsPresent(hTable, noteID))
		{
			//printf("* * Note %lX is already in the unread list\n",
				//pActions[index].NoteID);
		}
		else
		{
			/* make sure we check to see if this note really exists	at all */
			status = NSFNoteOpen(hDb, noteID, OPEN_SUMMARY, &hNote);

			/* if it does we'll add it to the unread list */
			if (status == NOERROR)
			{
				NSFNoteClose(hNote);
				status = IDInsert(hTable, noteID, (NOTESBOOL NOTESPTR) NULL);
			}
		}
	}
	else
	{
		/* Removing a Note ID */
		/* (Marks note as Read) */
		if (IDIsPresent(hTable, noteID))
		{
			status = IDDelete(hTable, noteID, (NOTESBOOL NOTESPTR) NULL);
		}
		//else
		//{
		//	printf("* * Note %lX is not in the unread list\n", pActions[index].NoteID);
		//}
	}

	if (NOERROR == status)
		status = NSFDbSetUnreadNoteTable(hDb, UserName, UserNameLen, TRUE, hOriginalTable, hTable);

	stat2 = IDDestroyTable(hOriginalTable);
	if (NOERROR == status)
		status = stat2;

	stat2 = IDDestroyTable(hTable);
	if (NOERROR == status)
		status = stat2;

	return status;
}

void __cdecl Process(char* inbuf, char* outbuf)
{
	
//	char tmp[BUF_LEN];
	char* data = NULL;
	ACTION action = UNKNOWN;
	STATUS      status = 0;
	NOTEID noteID;
	
	ZeroMemory(outbuf, BUF_LEN);
	int len = strlen(inbuf);
	if (inbuf[len - 1] == '\n')	inbuf[len - 1] = 0;
	if (inbuf[len - 2] == '\r')	inbuf[len - 2] = 0;
	SCAN_DATA(ID) else SCAN_DATA(PASS) else SCAN_DATA(DB) else SCAN_DATA(FOLDER) 
	else SCAN_DATA(READ) else SCAN_DATA(UNREAD) else SCAN_DATA(DOC) else SCAN_DATA(READ) else SCAN_DATA(UNREAD);

	switch (action)
	{
	case ID:
		strcpy_s(id_file, BUF_LEN, buf);
		if (status = NotesInitExtended(0, NULL))
		{
			strcpy_s(outbuf, BUF_LEN, "ERROR: Unable to initialize Notes.\n");
			return;
		}
		strcpy_s(outbuf, BUF_LEN, "OK\n");
		break;
	case PASS:
		CHECK(SECKFMSwitchToIDFile(id_file, buf, UserName, MAXUSERNAME, 0, NULL));
		UserNameLen = strlen(UserName);
		strcpy_s(outbuf, BUF_LEN, "OK\n");
		break;
	case DB:
		CHECK(NSFDbOpen(buf, &hDb));
		strcpy_s(outbuf, BUF_LEN, "OK\n");
		break;
	case FOLDER:
		CHECK(CountUnread(buf, outbuf));
		break;
	case DOC:
		sscanf_s(buf, "%x", &noteID);
		CHECK(IsDocUnread(noteID, outbuf));
		break;
	case READ:
		sscanf_s(buf, "%x", &noteID);
		CHECK(MarkDoc(noteID, false));
		strcpy_s(outbuf, BUF_LEN, "OK\n");
		break;
	case UNREAD:
		sscanf_s(buf, "%x", &noteID);
		CHECK(MarkDoc(noteID, true));
		strcpy_s(outbuf, BUF_LEN, "OK\n");
		break;
	default:
		strcpy_s(outbuf, BUF_LEN, "ERROR: Command unknown\n");
	}
	
}