#ifndef COMMON_STRUCT_H
#define COMMON_STRUCT_H

//���Ʊ�ʶ
enum
{
	SOCKET_SND = 1,//SOCKET ����
	SOCKET_SND_FINISH,//SOCKET �������
	SOCKET_REV,//SOCKET ����
	SOCKET_REV_FINISH,//SOCKET�������
	SOCKET_CLOSE,//SOCKET �˳�
};

//SOCKET �ص� IO �ṹ
struct sOverLapped		
{
	OVERLAPPED OverLapped;//�ص��ṹ
	WSABUF WSABuffer;//���ݻ���
	UINT uOperationType;//��������
	unsigned __int64 i64Index;//�ͻ���ΨһID
};

#endif