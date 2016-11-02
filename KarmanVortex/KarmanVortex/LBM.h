#pragma once
#include "vector3.h"
#include <math.h>

class CLBM
{
public:
	enum ACCESS {
		NOW,
		NEXT
	};

	typedef struct {
		int x, y , z;			//�i�q�_��
		int directionNum;		//���x�x�N�g���̐�
		double deltaTime;		//���ԍ���
		double deltaLength;		//��ԍ���
		double density;			//�������x[kg/m^3]
		double lambda;			//���ώ��R�s��[nm]
		CVector3<double>* velocity;		//�������x[m/s]
		double cld;				//��\����[m]
		double cv;				//��\���x[m/s]
		double M;				//���q��(��C�̏ꍇ�͋�C�̕��σ�������)[kg/mol]
		double T;				//���x[K]
		double mu;				//�S���W��[Pa�Es]
		double R;				//�����C�̒萔
	}LBMInfo;

	typedef struct {
		double* distribut;	//�����̕��z�֐��l
		double density;			//�����I���x
		CVector3<double> velocity;			//�����I���x
	}Point;

	LBMInfo info;
	double tau;				//�ɘa����
	double c;				//���q�̈ڗ����x
	Point* point;			//�i�q�_��̒l
	Point* point_next;		//���̎��Ԃ̒l
	CVector3<double>* e;	//���x�����x�N�g��
	double* w;				//�W��
	int stepNum = 0;		//�X�e�b�v��
	Point inflow;			//����
	double Re;				//���C�m���Y��

	int cx, cy,r;			//�~�̒��S�ʒu�A���a

public:
	CLBM(LBMInfo lbm);
	~CLBM();
	void setValue(int x, int y ,int z,int direct,double value,ACCESS type);
	Point* getPoint(int x, int y, int z,ACCESS type);
	//���t���z�֐�
	double calcPeq(Point* point,int a);
	//���z�֐�
	double calcPa(double peq,Point* point,int a);
	//�����I���͂Ƌ����I���x�̌v�Z�A�ݒ�
	void calcDensityAndVelocity(int x, int y, int z,ACCESS type);
	//1�X�e�b�v�v�Z
	void calcStep();
	//�f�[�^�̏�����
	void initData();
	//���E����
	double boundaryDistributGet(int x, int y, int z, int a, ACCESS type);
	Point* boundaryGet(int x, int y, int z,int a,ACCESS type);
	//�I�u�W�F�N�g�̋��E����
	Point* objectBoundyGet(int x, int y, int z, ACCESS type);
	//�I�u�W�F�N�g�̋��E�������K�v��
	bool isObject(int x, int y, int z);
	//���U���x�𔽓]
	int invertVelocity(int a);
	//���͂��v�Z
	double calcPresser(Point* point) {
		return (point->density * info.R * info.T) / info.M;
	}
};

