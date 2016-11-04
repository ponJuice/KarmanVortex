#include "LBM.h"
#include "Debug.h"

#ifdef _OPENMP
#include <omp.h>
#endif


CLBM::CLBM(LBMInfo lbm)
{
	//�ݒ���R�s�[
	info = lbm;

	//���q�̈ڗ����x�̌v�Z
	c = info.deltaLength / info.deltaTime;


	//�ɘa���Ԃ̌v�Z
	//tau = info.lambda / info.deltaTime;
	tau = 3.0*(info.mu / info.density)*(info.deltaTime / (info.deltaLength*info.deltaLength)) + 0.5;

	//�W���̐ݒ�
	w = new double[info.directionNum];
	w[0] = 4.0 / 9.0;
	w[1] = 1.0 / 9.0;
	w[2] = w[1];
	w[3] = w[1];
	w[4] = w[1];
	w[5] = 1.0 / 36.0;
	w[6] = w[5];
	w[7] = w[5];
	w[8] = w[5];

	//���x�����x�N�g���̃������m��
	e = new CVector3<double>[info.directionNum];
	//���x�����x�N�g���̏�����
	//�ڗ����x���g�p
	e[0].set(0, 0, 0);
	e[1].set(1, 0, 0);
	e[2].set(0, 1, 0);
	e[3].set(-1, 0, 0);
	e[4].set(0, -1, 0);
	e[5].set(1, 1, 0);
	e[6].set(-1, 1, 0);
	e[7].set(-1, -1, 0);
	e[8].set(1, -1, 0);


	//�i�q�_�̃������m��
	point = new Point[info.x*info.y*info.z];
	point_next = new Point[info.x*info.y*info.z];
	for (int n = 0; n < info.x*info.y*info.z; n++) {
		point[n].distribut = new double[info.directionNum];
		point_next[n].distribut = new double[info.directionNum];
	}

	inflow.density = info.density;
	inflow.velocity.copy(info.velocity);
	inflow.distribut = new double[info.directionNum];
	for (int a = 0; a < info.directionNum; a++) {
		inflow.distribut[a] = calcPeq(&inflow, a);
	}


	//�~
	cx = info.x / 6;
	cy = info.y / 2;
	r = 8;

	//�I�u�W�F�N�g���̏�����
	initData();

	//���C�m���Y�����v�Z
	Re = (info.density * info.cv * info.cld) / info.mu;
}


CLBM::~CLBM()
{
	for (int n = 0; n < info.x*info.y*info.z; n++) {
		delete[] point[n].distribut;
		delete[] point_next[n].distribut;
	}
	delete[] inflow.distribut;
	delete[] point;
	delete[] point_next;
	delete[] e;
	delete[] w;
}


void CLBM::setValue(int x, int y, int z, int direct, double value, CLBM::ACCESS type) {
	if (x < 0 || x >= info.x || y < 0 || y >= info.y) {
		//�i�q�O���Q�Ɓ��������Ȃ�
		return;
	}
	if (type == ACCESS::NOW)
		point[info.x * info.y * z + info.x * y + x].distribut[direct] = value;
	else
		point_next[info.x * info.y * z + info.x * y + x].distribut[direct] = value;
}
double CLBM::calcPeq(Point* point, int a) {
	CVector3<double> temp;
	temp.copy(&e[a]);

	temp.mult(c, &temp);

	//double e_dot_v = temp.dot(&point->velocity);		//���x�����x�N�g���Ƒ��x�x�N�g���̓���
	double e_dot_v = e[a].dot(&point->velocity);
	double v_dot_v = point->velocity.dot(&point->velocity);
	//���t���z�֐�
	/*double peq = w[a] * point->density*(1.0
		+ (3.0*e_dot_v) / (c*c)
		+ (9.0*e_dot_v*e_dot_v) / (2.0*c*c*c*c)
		- (3 * v_dot_v*v_dot_v) / (2.0*c*c));*/
	double peq = w[a] * point->density*(1.0
		+ (3.0*e_dot_v)
		+ (9.0/2.0)*(e_dot_v*e_dot_v)
		- (3.0/2.0)*v_dot_v);
	return peq;
}

double CLBM::calcPa(double peq,Point* point, int a) {
	//Point* temp = getPoint(x, y, z, ACCESS::NOW);
	//���z�֐�
	double pa = point->distribut[a] - (1.0 / tau)*(point->distribut[a] - peq);
	return pa;
}


void CLBM::calcStep() {
	/*�v�Z���@...
	/����i�q�_�̊e�����̒l�����߂邽�߂ɁA
	/���̎���̊i�q�_�̒l�����ɂ���
	/���ׂĂ̕����̒l�����߂���
	/���̓_�ł̋����I���͂Ƌ����I���x���v�Z����*/
	
	//printf("%d\n",stepNum);

	//���ׂĂ̊i�q�_�𑖍�����

#pragma omp parallel for
	for (int x = 0; x < info.x; x++) {
#pragma omp parallel for
		for (int y = 0; y < info.y; y++) {
#pragma omp parallel for
			for (int z = 0; z < info.z; z++) {
				//�I�u�W�F�N�g���ɓ����Ă��邩
				double _x = cx - x;
				double _y = cy - y;
				if (sqrt(_x*_x + _y*_y) <= r) {
					//�~�ɓ����Ă���
					continue;
				}
				if(x <= 0){
					//�������E�̏ꍇ�͖������Œl��ݒ�
					Point* p = getPoint(x, y, z, ACCESS::NEXT);
					p->density = inflow.density;
					p->velocity.copy(&inflow.velocity);
					for (int a = 0; a < info.directionNum; a++) {
						p->distribut[a] = inflow.distribut[a];
					}
					
					continue;
				}
				if (x >= info.x - 1) {
					continue;
				}
				if (y <= 0) {
					continue;
				}
				if (y >= info.y - 1) {
					continue;
				}
#pragma omp parallel for
				//�����Ɋւ��Čv�Z����
				for (int a = 0; a < info.directionNum; a++) {
					//�܂��v�Z�Ɏg�p����i�q�_���擾����
					int _x = x - e[a].get(CVector3<double>::Dim::X);
					int _y = y - e[a].get(CVector3<double>::Dim::Y);
					int _z = z - e[a].get(CVector3<double>::Dim::Z);
					//printf("a:%d\n \tx:%d _x:%d\n\ty:%d _y:%d\n\tz:%d _z:%d\n",a,x, _x,y, _y,z, _z)
					//printf("/* ------- info start ------- */\n");
					Debug::debugOutputLatticeInfo(x, y, z, a, _x, _y, _z);
					Point* p = getPoint(_x,_y,_z,ACCESS::NOW);
					Debug::debugOutputPointInfo(p);
					//printf("/* ------- info end ------- */\n\n");
					//���t���z�֐����v�Z����
					double peq = 0;
					double pa = 0;
					if (p != nullptr) {
						peq = calcPeq(p,a);
						//���z�֐����v�Z����
						pa = calcPa(peq,p, a);
					}
					else {
						pa = boundaryDistributGet(x, y, z, a, NOW);
					}
					//���̎��ԂɊi�[
					setValue(x, y, z, a, pa, ACCESS::NEXT);
				}
				//���ׂĂ̕����̕��z�֐������܂�����A�����I���x�Ƌ����I���x���v�Z
				calcDensityAndVelocity(x, y, z,ACCESS::NEXT);
			}
			//printf("\n");
		}
	}
#pragma omp parallel for
	//�������E�͑��x���z��0�ɂ���K�v������̂ōŌ�ɍs��
	for (int x = 0; x < info.x; x++) {
		//���o���E�͑��x���z��0�ɂȂ�悤�ݒ�
		Point* p = getPoint(x, 1, 0, ACCESS::NEXT);
		Point* np = getPoint(x, 0, 0, ACCESS::NEXT);
		np->density = p->density;
		np->velocity.copy(&p->velocity);
#pragma omp parallel for
		for (int a = 0; a < info.directionNum; a++) {
			np->distribut[a] = p->distribut[a];
		}
		//���o���E�͑��x���z��0�ɂȂ�悤�ݒ�
		p = getPoint(x, info.y - 2, 0, ACCESS::NEXT);
		np = getPoint(x, info.y - 1, 0, ACCESS::NEXT);
		np->density = p->density;
		np->velocity.copy(&p->velocity);
#pragma omp parallel for
		for (int a = 0; a < info.directionNum; a++) {
			np->distribut[a] = p->distribut[a];
		}
	}
#pragma omp parallel for
	for (int y = 0; y < info.y; y++) {
		//���o���E�͑��x���z��0�ɂȂ�悤�ݒ�
		Point* p = getPoint(info.x - 2, y, 0, ACCESS::NEXT);
		Point* np = getPoint(info.x - 1, y, 0, ACCESS::NEXT);
		np->density = p->density;
		np->velocity.copy(&p->velocity);
#pragma omp parallel for
		for (int a = 0; a < info.directionNum; a++) {
			np->distribut[a] = p->distribut[a];
		}
	}
	//�Q�Ƃ����݂ɓ���ւ��邱�ƂŎ��Ԃ�i�܂���
	Point* p_temp = point;
	point = point_next;
	point_next = p_temp;

	stepNum++;
}

void CLBM::initData() {

	for (int x = 0; x < info.x; x++) {
		for (int y = 0; y < info.y; y++) {
			for (int z = 0; z < info.z; z++) {
				//�~�ɓ����Ă��邩
				double _x = cx - x;
				double _y = cy - y;
				if (sqrt(_x*_x + _y*_y) <= r) {
					//�~�ɓ����Ă���
					continue;
				}
				//�����Ɋւ��Čv�Z����
				for (int a = 0; a < info.directionNum; a++) {
					Point* p = getPoint(x, y, z, NOW);
					p->density = inflow.density;
					p->velocity.copy(&inflow.velocity);
					//p->velocity.set(0, 0, 0);
					//p->distribut[a] = calcPeq(p, a);
					//Debug
					/*double r = info.cld / 2.0;	//���a
					double l = info.cld / (double)(info.y-1) * (double)y; //����
					double rl = abs(l - r);			//���S����̋���
					double u = info.velocity->get(0)*(1.0 - (rl*rl) / (r*r));
					p->velocity.set(u, 0, 0);
					/*double l = abs(info.cld/2.0 - (double)y/(double)(info.y-1) * info.cld);//���S����̋���
					p->velocity.setAt(0, info.velocity->get(CVector3<double>::Dim::X)*(1-(l*l)/((info.cld/2.0)*(info.cld/2.0))));*/
					//double peq = calcPeq(p, a);
					p->distribut[a] = inflow.distribut[a];
				}
				calcDensityAndVelocity(x, y, z, NOW);
			}
		}
	}

}

void CLBM::calcDensityAndVelocity(int x, int y, int z,CLBM::ACCESS type) {
	Point* p = getPoint(x, y, z, type);
	CVector3<double> temp;
	CVector3<double> temp_e;
	p->density = 0;
	p->velocity.set(0, 0, 0);
	//���́A���x�̌v�Z
	for (int a = 0; a < info.directionNum; a++) {
		//�����I���x�̌v�Z
		p->density += p->distribut[a];
		//�����I���x�̌v�Z
		temp_e.copy(&e[a]);
		temp_e.mult(c, &temp_e);
		//e[a].mult(p->distribut[a], &temp);
		temp_e.mult(p->distribut[a], &temp);
		p->velocity.add(&temp);
	}
	//�����I���x�̌v�Z
	p->velocity.div(p->density, &p->velocity);
}

int CLBM::invertVelocity(int a) {
	if (a == 0)
		return 0;
	int t = a - 2;
	if (a <= 4) {
		if (t <= 0) {
			return a + 2;
		}
		else {
			return a - 2;
		}
	}
	else if (a <= 8) {
		if (t <= 4) {
			return a + 2;
		}
		else {
			return a - 2;
		}
	}
}

CLBM::Point* CLBM::getPoint(int x, int y, int z,CLBM::ACCESS type) {
	if (x < 0 || x >= info.x || y < 0 || y >= info.y) {
		//�i�q�O���Q�Ɓ����E�����Œl���擾
		//return boundaryGet(x, y, z,a, type);
		return nullptr;
	}
	double _x = cx - x;
	double _y = cy - y;

	if (sqrt(_x*_x + _y*_y) <= r) {
		//�~�ɓ����Ă���
		return nullptr;
	}
	/*else if (isObject(x, y, z)){
	//�I�u�W�F�N�g��
	return &objectBound;
	*/
	if (type == ACCESS::NOW) {
		return &point[info.x * info.y * z + info.x * y + x];
	}
	else {
		return &point_next[info.x * info.y * z + info.x * y + x];

	}
}

double CLBM::boundaryDistributGet(int x, int y, int z, int a, ACCESS type)
{
	
	int _x = x - e[a].get(CVector3<double>::Dim::X);
	int _y = y - e[a].get(CVector3<double>::Dim::Y);
	int _z = z - e[a].get(CVector3<double>::Dim::Z);
	//�㉺�͎������E
	if (_y < 0 || _y >= info.y) {
		//return getPoint(x, y, z, type)->distribut[a];
		_y = info.y - abs(_y);
	}
	if (_x < 0 || _x >= info.x) {
		//_x = info.x - abs(_x);
		if (_x < 0) {
			//��l����
			return inflow.distribut[a];
		}
		else {
			//���o
			return getPoint(x, _y, z, type)->distribut[a];
		}
	}
	double __x = cx - _x;
	double __y = cy - _y;

	if (sqrt(__x*__x + __y*__y) <= r) {
		//�~�ɓ����Ă���
		a = invertVelocity(a);
		_x = x;
		_y = y;
		_z = z;
	}
	/*else {
	return getPoint(x, y, z,type);
	}*/
	Debug::debugOutputBoundaryInfo(x, y, z, _x, _y, _z);
	return getPoint(_x, _y, _z,type)->distribut[a];
}

CLBM::Point* CLBM::boundaryGet(int x, int y, int z,int a,CLBM::ACCESS type) {
	int _x = x - e[a].get(CVector3<double>::Dim::X);
	int _y = y - e[a].get(CVector3<double>::Dim::Y);
	int _z = z - e[a].get(CVector3<double>::Dim::Z);
	if (_x < 0 || _x >= info.x) {
		_x = info.x - abs(_x);
	}//�㉺��bounce-back
	if (_y < 0 || _y >= info.y) {
		//return nullptr;
		//_y = info.y - abs(y);
		a = invertVelocity(a);
		_x = x;
		_y = y;
		_z = z;
	}
	/*else {
		return getPoint(x, y, z,type);
	}*/
	Debug::debugOutputBoundaryInfo(x, y, z, _x, _y, _z);
	return getPoint(_x, _y, _z, type);
}

CLBM::Point* CLBM::objectBoundyGet(int x, int y, int z, ACCESS type)
{
	return nullptr;
}

bool CLBM::isObject(int x, int y, int z)
{
	//return false;
	/*double centorX = info.x/2.0, centorY = info.y/2.0, centorZ = 1;
	double lengthX = (double)x - centorX;
	double lengthY = (double)y - centorY;
	double lengthZ = (double)z - centorZ;*/
	int cx = info.x / 2;
	int cy = info.y / 2;
	return x >= (cx - 10) && x <= (cx + 10)
		&& y >= (cy - 10) && y <= (cy + 10);
}
