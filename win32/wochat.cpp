#include "pch.h"
#include "mosquitto.h"
#include "wochat.h"
#include "mbedtls/base64.h"

int PushSendMessageQueue(MessageTask* message_task)
{
	MQTTPubData* pd;
	MessageTask* p;
	MessageTask* q;

	EnterCriticalSection(&g_csMQTTPub);
	pd = &g_PubData;

	if (nullptr == pd->task) // there is no task in the queue
	{
		pd->task = message_task;
		LeaveCriticalSection(&g_csMQTTPub);
		SetEvent(g_MQTTPubEvent); // tell the Pub thread to work
		return 0;
	}
	
	q = pd->task;
	while (pd->task)
	{
		if (pd->task->state < 2) // this task has been processed, we can free the memory
		{
			break;
		}
		pd->task = pd->task->next;
	}

	// here, pd->task is pointing to a node not procesded or NULL
	if (nullptr == pd->task) // there is no task in the queue
	{
		pd->task = message_task;
	}

	p = q; // p is pointing to the first node
	while (p)
	{
		q = p->next;
		if (nullptr == q) // we reach the end of the queue
			p->next = message_task;
	
		if (p->state > 1) // this task has been processed, we can free the memory
		{
			pfree(p->message);
			pfree(p);
		}
		p = q;
	}

	LeaveCriticalSection(&g_csMQTTPub);

	SetEvent(g_MQTTPubEvent); // tell the Pub thread to work

	return 0;
}

int Raw2HexString(U8* input, U8 len, U8* output, U8* outlen)
{
	U8 idx, i;
	const U8* hex_chars = (const U8*)"0123456789ABCDEF";

	for (i = 0; i < len; i++)
	{
		idx = ((input[i] >> 4) & 0x0F);
		output[(i << 1)] = hex_chars[idx];

		idx = (input[i] & 0x0F);
		output[(i << 1) + 1] = hex_chars[idx];
	}

	output[(i << 1)] = 0;
	if (outlen)
		*outlen = (i << 1);

	return 0;
}

int HexString2Raw(U8* input, U8 len, U8* output, U8* outlen)
{
	U8 oneChar, hiValue, lowValue, i;

	for (i = 0; i < len; i += 2)
	{
		oneChar = input[i];
		if (oneChar >= '0' && oneChar <= '9')
			hiValue = oneChar - '0';
		else if (oneChar >= 'A' && oneChar <= 'F')
			hiValue = (oneChar - 'A') + 0x0A;
		else return 1;

		oneChar = input[i + 1];
		if (oneChar >= '0' && oneChar <= '9')
			lowValue = oneChar - '0';
		else if (oneChar >= 'A' && oneChar <= 'F')
			lowValue = (oneChar - 'A') + 0x0A;
		else return 1;

		output[(i >> 1)] = (hiValue << 4 | lowValue);
	}

	if (outlen)
		*outlen = (len >> 1);

	return 0;
}

int Raw2HexStringW(U8* input, U8 len, wchar_t* output, U8* outlen)
{
	U8 idx, i;
	const wchar_t* hex_chars = (const wchar_t*)(L"0123456789ABCDEF");

	for (i = 0; i < len; i++)
	{
		idx = ((input[i] >> 4) & 0x0F);
		output[(i << 1)] = hex_chars[idx];

		idx = (input[i] & 0x0F);
		output[(i << 1) + 1] = hex_chars[idx];
	}

	output[(i << 1)] = 0;
	if (outlen)
		*outlen = (i << 1);

	return 0;
}

int HexString2RawW(wchar_t* input, U8 len, U8* output, U8* outlen)
{
	U8 oneChar, hiValue, lowValue, i;

	for (i = 0; i < len; i += 2)
	{
		oneChar = input[i];
		if (oneChar >= '0' && oneChar <= '9')
			hiValue = oneChar - '0';
		else if (oneChar >= 'A' && oneChar <= 'F')
			hiValue = (oneChar - 'A') + 0x0A;
		else return 1;

		oneChar = input[i + 1];
		if (oneChar >= '0' && oneChar <= '9')
			lowValue = oneChar - '0';
		else if (oneChar >= 'A' && oneChar <= 'F')
			lowValue = (oneChar - 'A') + 0x0A;
		else return 1;

		output[(i >> 1)] = (hiValue << 4 | lowValue);
	}

	if (outlen)
		*outlen = (len >> 1);

	return 0;
}


int AESEncrypt(U8* input, int length, U8* output)
{
	mbedtls_aes_context master;
	mbedtls_aes_init(&master);

	return 0;
}

#define SQL_STMT_MAX_LEN		1024

int InitWoChatDatabase(LPCWSTR lpszPath)
{
	sqlite3* db;
	sqlite3_stmt* stmt = NULL;
	int           rc;
	wchar_t keyFile[MAX_PATH + 1] = { 0 };
	wchar_t sql[SQL_STMT_MAX_LEN] = { 0 };

	swprintf((wchar_t*)keyFile, MAX_PATH, L"%s\\wt.db", lpszPath);

	rc = sqlite3_open16(keyFile, &db);

	if (rc) 
	{
		sqlite3_close(db);
		return -1;
	}

	_stprintf_s(sql, SQL_STMT_MAX_LEN,
		L"CREATE TABLE config(id INTEGER PRIMARY KEY, idx INTEGER, intv INTEGER, charv VARCHAR(2048))");
	rc = sqlite3_prepare16_v2(db, sql, -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	rc = sqlite3_finalize(stmt);

	sqlite3_close(db);

	return 0;
}

extern "C"
{
	static void MQTT_Message_Callback(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message, const mosquitto_property* properties);
	static void MQTT_Connect_Callback(struct mosquitto* mosq, void* obj, int result, int flags, const mosquitto_property* properties);
	static void MQTT_Disconnect_Callback(struct mosquitto* mosq, void* obj, int result, const mosquitto_property* properties);
	static void MQTT_Publish_Callback(struct mosquitto* mosq, void* obj, int mid, int reason_code, const mosquitto_property* properties);
	static void MQTT_Subscribe_Callback(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos);
	static void MQTT_Log_Callback(struct mosquitto* mosq, void* obj, int level, const char* str);

	static MQTT_Methods mqtt_callback =
	{
		MQTT_Message_Callback,
		MQTT_Connect_Callback,
		MQTT_Disconnect_Callback,
		MQTT_Subscribe_Callback,
		MQTT_Publish_Callback,
		MQTT_Log_Callback
	};

	static XMQTTMessage mqtt_message = { 0 };
}

#if 0
int ConvertBinToHex(U8* pk, U16 len, U8* pkHex)
{
	int ret = 0;

	if (len == 33)
	{
		U8 oneChar;
		for (U16 i = 0; i < len; i++)
		{
			oneChar = (pk[i] >> 4);
			if(oneChar < 0x0A)
				pkHex[(i << 1)] = oneChar + '0';
			else
				pkHex[(i << 1)] = (oneChar - 0x0A) + 'A';

			oneChar = (pk[i] & 0x0F);
			if (oneChar < 0x0A)
				pkHex[(i << 1)+1] = oneChar + '0';
			else
				pkHex[(i << 1)+1] = (oneChar - 0x0A) + 'A';
		}
		pkHex[66] = 0;
	}

	return ret;
}
#endif

int GetKeys(LPCTSTR path, U8* sk, U8* pk)
{
	int ret = 1;
	U8  hexSK[32];
	wchar_t keyFile[MAX_PATH + 1] = { 0 };
	
	swprintf((wchar_t*)keyFile, MAX_PATH, L"%s\\keys.txt", path);

	DWORD fileAttributes = GetFileAttributesW(keyFile);
	if (fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		int  fd;
		if (0 == _tsopen_s(&fd, keyFile, _O_RDONLY | _O_BINARY, _SH_DENYWR, 0))
		{
			U32  size, bytes;
			U8*  p;
			U8   hiChar, lowChar, hiValue, lowValue;
			U8   buf[256];

			size = (U32)_lseek(fd, 0, SEEK_END); /* get the file size */
			if (size < 131)
			{
				_close(fd);
				return 1;
			}
			_lseek(fd, 0, SEEK_SET); /* go to the begin of the file */
			bytes = (U32)_read(fd, buf, 131);  /* try to detect PNG header */
			if(bytes != 131)
			{
				_close(fd);
				return 1;
			}
			_close(fd);

			if (buf[64] != '\n')
				return 1;

			p = buf;
			for (int i = 0; i < 32; i++)
			{
				hiChar = p[(i << 1)];
				if (hiChar >= '0' && hiChar <= '9')
					hiValue = hiChar - '0';
				else if (hiChar >= 'A' && hiChar <= 'F')
					hiValue = (hiChar - 'A') + 0x0A;
				else return 1;

				lowChar = p[(i << 1) + 1];
				if (lowChar >= '0' && lowChar <= '9')
					lowValue = lowChar - '0';
				else if (lowChar >= 'A' && lowChar <= 'F')
					lowValue = (lowChar - 'A') + 0x0A;
				else return 1;

				sk[i] = (hiValue << 4 | lowValue);
			}

			p = buf + 65;
			for (int i = 0; i < 33; i++)
			{
				hiChar = p[(i << 1)];
				if (hiChar >= '0' && hiChar <= '9')
					hiValue = hiChar - '0';
				else if (hiChar >= 'A' && hiChar <= 'F')
					hiValue = (hiChar - 'A') + 0x0A;
				else return 1;

				lowChar = p[(i << 1) + 1];
				if (lowChar >= '0' && lowChar <= '9')
					lowValue = lowChar - '0';
				else if (lowChar >= 'A' && lowChar <= 'F')
					lowValue = (lowChar - 'A') + 0x0A;
				else return 1;

				pk[i] = (hiValue << 4 | lowValue);
			}
			ret = 0;
		}
	}

#if 0
	if (!bRet)
	{
		int fd;
		U8  len, oneChar;
		U8  hexString[65];
		U8 text[256 + 1] = { 0 };

		NTSTATUS status = BCryptGenRandom(NULL, (PUCHAR)hexSK, 32, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
		if (STATUS_SUCCESS != status)
			return 1;

		if (0 != _tsopen_s(&fd, iniFile, _O_CREAT | _O_WRONLY | _O_TRUNC | _O_TEXT, _SH_DENYWR, 0))
			return 2;
		for (int i = 0; i < 32; i++)
		{
			sk[i] = hexSK[i];
			oneChar = (sk[i] >> 4) & 0x0F;
			if (oneChar < 0x0A)
				hexString[(i << 1)] = oneChar + '0';
			else
				hexString[(i << 1)] = (oneChar - 0x0A) + 'A';

			oneChar = sk[i] & 0x0F;
			if (oneChar < 0x0A)
				hexString[(i << 1) + 1] = oneChar + '0';
			else
				hexString[(i << 1) + 1] = (oneChar - 0x0A) + 'A';
		}
		hexString[64] = 0;

		sprintf_s((char*)text, 256, "[global]\nSK=%s\n", hexString);
		len = strlen((const char*)text);
		_write(fd, text, len);
		_close(fd);
	}
#endif
    return ret;
}


DWORD WINAPI MQTTSubThread(LPVOID lpData)
{
	int ret = 0;
	Mosquitto mq;
	HWND hWndUI;

	InterlockedIncrement(&g_threadCount);

	hWndUI = (HWND)(lpData);
	ATLASSERT(::IsWindow(hWndUI));

	mq = MQTT::MQTT_SubInit(hWndUI, MQTT_DEFAULT_HOST, MQTT_DEFAULT_PORT, &mqtt_callback);

	if (nullptr == mq) // something is wrong in MQTT sub routine
	{
		PostMessage(hWndUI, WM_MQTT_SUBMESSAGE, 1, 0);
		goto QuitMQTTSubThread;
	}

	ret = MQTT::MQTT_AddSubTopic(CLIENT_SUB, "03342EF59638622ABEE545AFABA85463932DA66967F2BA4EE6254F9988BEE5F0C2");
	if(ret)
	{
		PostMessage(hWndUI, WM_MQTT_SUBMESSAGE, 2, 0);
		goto QuitMQTTSubThread;
	}

	MQTT::MQTT_SubLoop(mq, &g_Quit);  // main loop go here.
	
QuitMQTTSubThread:
	MQTT::MQTT_SubTerm(mq);
	InterlockedDecrement(&g_threadCount);
	return 0;
}

DWORD WINAPI MQTTPubThread(LPVOID lpData)
{
	int ret = 0;
	DWORD dwRet;
	HWND hWndUI;
	Mosquitto mq;
	MemoryPoolContext mempool;
	MessageTask* p;
	char topic[67] = { 0 };
	char* msgbody = nullptr;
	int   msglen = 0;
	size_t encoded_len;
	size_t output_len;
	char* encoded_string;

	InterlockedIncrement(&g_threadCount);

	hWndUI = (HWND)(lpData);
	ATLASSERT(::IsWindow(hWndUI));

	mq = MQTT::MQTT_PubInit(hWndUI, MQTT_DEFAULT_HOST, MQTT_DEFAULT_PORT, &mqtt_callback);

	if (nullptr == mq)
	{
		PostMessage(hWndUI, WM_MQTT_PUBMESSAGE, 1, 0);
		goto QuitMQTTPubThread;
	}

	mempool = mempool_create("MQTT_SUB_POOL", 0, DUI_ALLOCSET_DEFAULT_INITSIZE, DUI_ALLOCSET_DEFAULT_MAXSIZE);
	if (nullptr == mempool)
	{
		PostMessage(hWndUI, WM_MQTT_PUBMESSAGE, 2, 0);
		goto QuitMQTTPubThread;
	}

	while (true)
	{
		dwRet = WaitForSingleObject(g_MQTTPubEvent, INFINITE);

		if (dwRet != WAIT_OBJECT_0)
			continue;
		if (g_Quit) // we will quit the whole program
			break;

		EnterCriticalSection(&g_csMQTTPub);
		p = g_PubData.task;
		LeaveCriticalSection(&g_csMQTTPub);

		while (p)
		{
			Raw2HexString(p->pubkey, 33, (U8*)topic, nullptr);

			encoded_len = mbedtls_base64_encode(NULL, 0, &output_len, (unsigned char*)p->message, (p->msgLen << 1));
			encoded_string = (char*)palloc(mempool, output_len + 1);
			if (encoded_string)
			{
				mbedtls_base64_encode((unsigned char*)encoded_string, output_len, &output_len, (unsigned char*)p->message, p->msgLen);
				MQTT::MQTT_PubMessage(mq, topic, encoded_string, output_len);
				pfree(encoded_string);
			}
			InterlockedIncrement(&(p->state)); // we have processed this task.
			p = p->next;
		}
	}

	mempool_destroy(mempool);

QuitMQTTPubThread:

	MQTT::MQTT_PubTerm(mq);

	InterlockedDecrement(&g_threadCount);
	return 0;
}

extern "C"
{
	static bool process_messages = true;
	static int msg_count = 0;
	static int last_mid = 0;
	static bool timed_out = false;
	static int connack_result = 0;
	static bool connack_received = false;

	static int PostMQTTMessage(HWND hWnd, const struct mosquitto_message* message, const mosquitto_property* properties)
	{
		U8* p;
		U8* msg;
		size_t len;
		size_t bytes;

		assert(nullptr != message);

		msg = (U8*)message->payload;
		assert(nullptr != msg);
		len = (size_t)message->payloadlen;
		assert(len > 0);

		if (::IsWindow(hWnd))
		{
			///for (size_t i = 0; i < len; i++)
				///xmsgUTF8GET[i] = msg[i];
			///::PostMessage(hWnd, WM_MQTT_SUBMSG, (WPARAM)xmsgUTF8GET, (LPARAM)len);
		}
		return 0;
	}


	static void MQTT_Message_Callback(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message, const mosquitto_property* properties)
	{
		int i;
		bool res;
		struct mosq_config* pMQTTConf;
		HWND hWnd;

		UNUSED(properties);

		pMQTTConf = (struct mosq_config*)obj;
		assert(nullptr != pMQTTConf);

		hWnd = (HWND)(pMQTTConf->userdata);
		assert(::IsWindow(hWnd));

		if (process_messages == false) return;

		if (pMQTTConf->retained_only && !message->retain && process_messages)
		{
			process_messages = false;
			if (last_mid == 0)
			{
				mosquitto_disconnect_v5(mosq, 0, pMQTTConf->disconnect_props);
			}
			return;
		}

		if (message->retain && pMQTTConf->no_retain)
			return;

		if (pMQTTConf->filter_outs)
		{
			for (i = 0; i < pMQTTConf->filter_out_count; i++)
			{
				mosquitto_topic_matches_sub(pMQTTConf->filter_outs[i], message->topic, &res);
				if (res) return;
			}
		}

		if (pMQTTConf->remove_retained && message->retain)
		{
			mosquitto_publish(mosq, &last_mid, message->topic, 0, NULL, 1, true);
		}

		PostMQTTMessage(hWnd, message, properties);

		if (pMQTTConf->msg_count > 0)
		{
			msg_count++;
			if (pMQTTConf->msg_count == msg_count)
			{
				process_messages = false;
				if (last_mid == 0)
				{
					mosquitto_disconnect_v5(mosq, 0, pMQTTConf->disconnect_props);
				}
			}
		}
	}

	static void MQTT_Connect_Callback(struct mosquitto* mosq, void* obj, int result, int flags, const mosquitto_property* properties)
	{
		int i;
		struct mosq_config* pMQTTConf;

		UNUSED(flags);
		UNUSED(properties);

		pMQTTConf = (struct mosq_config*)obj;
		assert(nullptr != pMQTTConf);

		//connack_received = true;

		//connack_result = result;

		if (!result)
		{
			mosquitto_subscribe_multiple(mosq, NULL, pMQTTConf->topic_count, pMQTTConf->topics, pMQTTConf->qos, pMQTTConf->sub_opts, pMQTTConf->subscribe_props);

			for (i = 0; i < pMQTTConf->unsub_topic_count; i++)
			{
				mosquitto_unsubscribe_v5(mosq, NULL, pMQTTConf->unsub_topics[i], pMQTTConf->unsubscribe_props);
			}
		}
		else
		{
			if (result)
			{
				if (pMQTTConf->protocol_version == MQTT_PROTOCOL_V5)
				{
					if (result == MQTT_RC_UNSUPPORTED_PROTOCOL_VERSION)
					{
						//err_printf(&cfg, "Connection error: %s. Try connecting to an MQTT v5 broker, or use MQTT v3.x mode.\n", mosquitto_reason_string(result));
					}
					else
					{
						//err_printf(&cfg, "Connection error: %s\n", mosquitto_reason_string(result));
					}
				}
				else {
					//err_printf(&cfg, "Connection error: %s\n", mosquitto_connack_string(result));
				}
			}
			mosquitto_disconnect_v5(mosq, 0, pMQTTConf->disconnect_props);
		}
	}

	static void MQTT_Disconnect_Callback(struct mosquitto* mosq, void* obj, int result, const mosquitto_property* properties)
	{

	}

	static void MQTT_Publish_Callback(struct mosquitto* mosq, void* obj, int mid, int reason_code, const mosquitto_property* properties)
	{

	}

	static void MQTT_Subscribe_Callback(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos)
	{
		int i;
		struct mosq_config* pMQTTConf;
		bool some_sub_allowed = (granted_qos[0] < 128);
		bool should_print;

		pMQTTConf = (struct mosq_config*)obj;
		assert(nullptr != pMQTTConf);
		should_print = pMQTTConf->debug && !pMQTTConf->quiet;

#if 0
		if (should_print)
			printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
#endif
		for (i = 1; i < qos_count; i++)
		{
			//if (should_print) printf(", %d", granted_qos[i]);
			some_sub_allowed |= (granted_qos[i] < 128);
		}
		//if (should_print) printf("\n");

		if (some_sub_allowed == false)
		{
			mosquitto_disconnect_v5(mosq, 0, pMQTTConf->disconnect_props);
			//err_printf(&cfg, "All subscription requests were denied.\n");
		}

		if (pMQTTConf->exit_after_sub)
		{
			mosquitto_disconnect_v5(mosq, 0, pMQTTConf->disconnect_props);
		}

	}

	static void MQTT_Log_Callback(struct mosquitto* mosq, void* obj, int level, const char* str)
	{

	}
}

