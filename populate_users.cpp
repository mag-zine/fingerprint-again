#include <iostream>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "include/sgfplib.h"

int   msg_qid;
key_t key;
struct msgbuf qbuf;


LPSGFPM  sgfplib = NULL;

bool StartAutoOn(LPSGFPM m_sgfplib)
{
  DWORD result;
  bool StartAutoOn = false;
  // Start Message Queue
  // Create unique key via call to ftok() 

  printf("Create unique message key\n");
  key = ftok(".", 'a'); //'a' is an arbitrary seed value
  // Open the queue - create if necessary 

  if((msg_qid = msgget(key, IPC_CREAT|0660)) == -1)
    return false;

  printf("Message Queue ID is : %d\n",msg_qid);
  // Start Message Queue ///////////////////////////////////////////////////

  // EnableAutoOnEvent(true)
  printf("Call sgfplib->EnableAutoOnEvent(true) ... \n");  
  result = m_sgfplib->EnableAutoOnEvent(true,&msg_qid,NULL);
  printf("sgfplib->EnableAutoOnEvent()  returned ... ");  
  if (result != SGFDX_ERROR_NONE)
  {
     printf("FAIL - [%ld]\n",result);  
  }
  else
  {
     StartAutoOn = true;
     printf("SUCCESS - [%ld]\n",result);  
  }
  printf(".............................................................\n");  
  return StartAutoOn;
}

bool StopAutoOn(LPSGFPM m_sgfplib)
{
  DWORD result;
  bool StopAutoOn = false;
  //////////////////////////////////////////////////////////////////////////
  // EnableAutoOnEvent(false)
  printf("Calling ISensor::EnableAutoOnEvent(false) ... \n");  
  result = m_sgfplib->EnableAutoOnEvent(false,&msg_qid,NULL);
  printf("sgfplib->EnableAutoOnEvent(false)  returned ... ");  
  if (result != SGFDX_ERROR_NONE)
  {
     printf("FAIL - [%ld]\n",result);  
  }
  else
  {
     StopAutoOn = true;
     printf("SUCCESS - [%ld]\n",result);  
  }
  printf(".............................................................\n");  

  //////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////
  // Remove Message Queue //////////////////////////////////////////////////
  msgctl(msg_qid, IPC_RMID, 0);
  // Remove Message Queue //////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////

  return StopAutoOn;

}

long fingerPresent()
{
	int fingerPresent = 0;
	//printf("Reading message queue ...\n");

	#ifdef _U20
		qbuf.mtype = FDU05_MSG;
	#endif
	#ifdef _FDU04
		qbuf.mtype = FDU04_MSG;
	#endif
	#ifdef _FDU03
		qbuf.mtype = FDU03_MSG;
	#endif

	msgrcv(msg_qid, (struct msgbuf *)&qbuf, MAX_SEND_SIZE, qbuf.mtype, 0);
	//printf("Type: %ld Text: %s\n", qbuf.mtype, qbuf.mtext);
	if (strlen(qbuf.mtext) > 0)
	{
	fingerPresent= atol(qbuf.mtext);
	}
	return fingerPresent;
}


int main(int argc, char* argv[]){
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	long err;
	int fingerLength = 0;
	BYTE* imageBuffer1;
	BYTE *minutiaeBuffer1, *minutiaeBuffer2; 
	DWORD quality;
	DWORD templateSize, templateSizeMax;
	SGFingerInfo fingerInfo;
	DWORD score;
	key_t key;
	int   msg_qid;
	struct msgbuf qbuf;
	SGDeviceInfoParam deviceInfo;
	int status;
	std::string sqlQuery;

	// Instantiate SGFPLib object
	err = CreateSGFPMObject(&sgfplib);
	if (!sgfplib){
		printf("ERROR - Unable to instantiate FPM object\n");
		return false;
	}
	printf("CreateSGFPMObject returned: %ld\n",err);

	if (err == SGFDX_ERROR_NONE){

		// Init()
		printf("Call sgfplib->Init(SG_DEV_AUTO)\n");
		err = sgfplib->Init(SG_DEV_AUTO);
		printf("Init returned: %ld\n",err);

		// OpenDevice()
		printf("Call sgfplib->OpenDevice(0)\n");
		err = sgfplib->OpenDevice(0);
		printf("OpenDevice returned : [%ld]\n\n",err);

		// getDeviceInfo()
		err = sgfplib->GetDeviceInfo(&deviceInfo);
		printf("GetDeviceInfo returned: %ld\n\n",err);

		imageBuffer1 = (BYTE*) malloc(deviceInfo.ImageWidth*deviceInfo.ImageHeight);

		if (StartAutoOn(sgfplib)){  
			while (1){
				if (fingerPresent()){
					printf("Finger Present\n");
					if (!StopAutoOn(sgfplib)){
						printf("StopAutoOn() returned false.\n");
						break;                
					}
					printf("Call ISensor::GetImage()\n");
					err = sgfplib->GetImage(imageBuffer1);
					printf("ISensor::GetImage() returned ... ");  
					if (err != SGFDX_ERROR_NONE){
						printf("FAIL - [%ld]\n",err);
					}
					else{
						printf("SUCCESS - [%ld]\n",err);

					    // SetTemplateFormat(TEMPLATE_FORMAT_SG400)
					    err = sgfplib->SetTemplateFormat(TEMPLATE_FORMAT_SG400);

					    // getMaxTemplateSize()
					    err = sgfplib->GetMaxTemplateSize(&templateSizeMax);

					    // getMinutiae()
					    minutiaeBuffer1 = (BYTE*) malloc(templateSizeMax);
					    minutiaeBuffer2 = (BYTE*) malloc(templateSizeMax);

					    fingerInfo.FingerNumber = SG_FINGPOS_UK;
					    fingerInfo.ViewNumber = 1;
					    fingerInfo.ImpressionType = SG_IMPTYPE_LP;
					    fingerInfo.ImageQuality = quality; //0 to 100
					    err = sgfplib->CreateTemplate(&fingerInfo, imageBuffer1, minutiaeBuffer1);

					    if (err == SGFDX_ERROR_NONE){
							// getTemplateSize()
							err = sgfplib->GetTemplateSize(minutiaeBuffer1, &templateSize);

							sqlQuery = "insert into regUser values (?,?,?);";
							printf("%s",minutiaeBuffer1);

							if(sqlite3_open("sqldb.db",&db) == SQLITE_OK){
								sqlite3_prepare_v2(db,sqlQuery.c_str(),-1,&stmt, NULL);
								sqlite3_bind_int(stmt,1,atoi(argv[1]));
								//sqlite3_bind_int(stmt,2,status);
								sqlite3_bind_blob(stmt,2,minutiaeBuffer1,4096,SQLITE_TRANSIENT);
								sqlite3_bind_text(stmt,3,argv[2],-1,SQLITE_TRANSIENT);


								if(sqlite3_step(stmt)!=SQLITE_DONE)
									printf("Errata");
								else
									printf("data insertion successful %d %d\n",sizeof(minutiaeBuffer1), sizeof(*minutiaeBuffer1));
							}
							else{
								printf("failed to open db, create db? y/n");
							}
							sqlite3_finalize(stmt);
							sqlite3_close(db);
						}
						printf(".............................................................\n");
						printf("Press 'X' to exit, any other key to continue >> ");
						if (getc(stdin) == 'X')
							break;
						if(!StartAutoOn(sgfplib)){
							printf("StartAutoOn() returned false.\n");
							break;
						}
					}
				}
			}

			// EnableAutoOnEvent(false)
			printf("Call sgfplib->EnableAutoOnEvent(false) ... \n");
			err = sgfplib->EnableAutoOnEvent(false,&msg_qid,NULL);
			printf("EnableAutoOnEvent returned : [%ld]\n", err);

			// Remove Message Queue 
			msgctl(msg_qid, IPC_RMID, 0);

			// closeDevice()
			printf("\nCall fplib->CloseDevice()\n");
			err = sgfplib->CloseDevice();
			printf("CloseDevice returned : [%ld]\n",err);

			// Destroy FPLib object
			printf("\nCall DestroySGFPMObject(fplib)\n");
			err = DestroySGFPMObject(sgfplib);
			printf("DestroySGFPMObject returned : [%ld]\n",err);

			free(imageBuffer1);
			imageBuffer1 = NULL;

		}
	}
	return 0;
}
