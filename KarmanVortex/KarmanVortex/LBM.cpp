#include "LBM.h"
#include "Debug.h"

#ifdef _OPENMP
#include <omp.h>
#endif


CLBM::CLBM(LBMInfo lbm)
{
	//設定をコピー
	info = lbm;

	//粒子の移流速度の計算
	c = info.deltaLength / info.deltaTime;


	//緩和時間の計算
	//tau = info.lambda / info.deltaTime;
	tau = 3.0*(info.mu / info.density)*(info.deltaTime / (info.deltaLength*info.deltaLength)) + 0.5;

	//係数の設定
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

	//速度方向ベクトルのメモリ確保
	e = new CVector3<double>[info.directionNum];
	//速度方向ベクトルの初期化
	//移流速度を使用
	e[0].set(0, 0, 0);
	e[1].set(1, 0, 0);
	e[2].set(0, 1, 0);
	e[3].set(-1, 0, 0);
	e[4].set(0, -1, 0);
	e[5].set(1, 1, 0);
	e[6].set(-1, 1, 0);
	e[7].set(-1, -1, 0);
	e[8].set(1, -1, 0);


	//格子点のメモリ確保
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


	//円
	cx = info.x / 6;
	cy = info.y / 2;
	r = 8;

	//オブジェクト内の初期化
	initData();

	//レイノルズ数を計算
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
		//格子外を参照→何もしない
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

	//double e_dot_v = temp.dot(&point->velocity);		//速度方向ベクトルと速度ベクトルの内積
	double e_dot_v = e[a].dot(&point->velocity);
	double v_dot_v = point->velocity.dot(&point->velocity);
	//平衡分布関数
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
	//分布関数
	double pa = point->distribut[a] - (1.0 / tau)*(point->distribut[a] - peq);
	return pa;
}


void CLBM::calcStep() {
	/*計算方法...
	/ある格子点の各方向の値を求めるために、
	/その周りの格子点の値を元にする
	/すべての方向の値を求めたら
	/その点での巨視的圧力と巨視的速度を計算する*/
	
	//printf("%d\n",stepNum);

	//すべての格子点を走査する

#pragma omp parallel for
	for (int x = 0; x < info.x; x++) {
#pragma omp parallel for
		for (int y = 0; y < info.y; y++) {
#pragma omp parallel for
			for (int z = 0; z < info.z; z++) {
				//オブジェクト内に入っているか
				double _x = cx - x;
				double _y = cy - y;
				if (sqrt(_x*_x + _y*_y) <= r) {
					//円に入っている
					continue;
				}
				if(x <= 0){
					//流入境界の場合は無条件で値を設定
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
				//方向に関して計算する
				for (int a = 0; a < info.directionNum; a++) {
					//まず計算に使用する格子点を取得する
					int _x = x - e[a].get(CVector3<double>::Dim::X);
					int _y = y - e[a].get(CVector3<double>::Dim::Y);
					int _z = z - e[a].get(CVector3<double>::Dim::Z);
					//printf("a:%d\n \tx:%d _x:%d\n\ty:%d _y:%d\n\tz:%d _z:%d\n",a,x, _x,y, _y,z, _z)
					//printf("/* ------- info start ------- */\n");
					Debug::debugOutputLatticeInfo(x, y, z, a, _x, _y, _z);
					Point* p = getPoint(_x,_y,_z,ACCESS::NOW);
					Debug::debugOutputPointInfo(p);
					//printf("/* ------- info end ------- */\n\n");
					//平衡分布関数を計算する
					double peq = 0;
					double pa = 0;
					if (p != nullptr) {
						peq = calcPeq(p,a);
						//分布関数を計算する
						pa = calcPa(peq,p, a);
					}
					else {
						pa = boundaryDistributGet(x, y, z, a, NOW);
					}
					//次の時間に格納
					setValue(x, y, z, a, pa, ACCESS::NEXT);
				}
				//すべての方向の分布関数が求まったら、巨視的速度と巨視的密度を計算
				calcDensityAndVelocity(x, y, z,ACCESS::NEXT);
			}
			//printf("\n");
		}
	}
#pragma omp parallel for
	//流入境界は速度勾配を0にする必要があるので最後に行う
	for (int x = 0; x < info.x; x++) {
		//流出境界は速度勾配が0になるよう設定
		Point* p = getPoint(x, 1, 0, ACCESS::NEXT);
		Point* np = getPoint(x, 0, 0, ACCESS::NEXT);
		np->density = p->density;
		np->velocity.copy(&p->velocity);
#pragma omp parallel for
		for (int a = 0; a < info.directionNum; a++) {
			np->distribut[a] = p->distribut[a];
		}
		//流出境界は速度勾配が0になるよう設定
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
		//流出境界は速度勾配が0になるよう設定
		Point* p = getPoint(info.x - 2, y, 0, ACCESS::NEXT);
		Point* np = getPoint(info.x - 1, y, 0, ACCESS::NEXT);
		np->density = p->density;
		np->velocity.copy(&p->velocity);
#pragma omp parallel for
		for (int a = 0; a < info.directionNum; a++) {
			np->distribut[a] = p->distribut[a];
		}
	}
	//参照を交互に入れ替えることで時間を進ませる
	Point* p_temp = point;
	point = point_next;
	point_next = p_temp;

	stepNum++;
}

void CLBM::initData() {

	for (int x = 0; x < info.x; x++) {
		for (int y = 0; y < info.y; y++) {
			for (int z = 0; z < info.z; z++) {
				//円に入っているか
				double _x = cx - x;
				double _y = cy - y;
				if (sqrt(_x*_x + _y*_y) <= r) {
					//円に入っている
					continue;
				}
				//方向に関して計算する
				for (int a = 0; a < info.directionNum; a++) {
					Point* p = getPoint(x, y, z, NOW);
					p->density = inflow.density;
					p->velocity.copy(&inflow.velocity);
					//p->velocity.set(0, 0, 0);
					//p->distribut[a] = calcPeq(p, a);
					//Debug
					/*double r = info.cld / 2.0;	//半径
					double l = info.cld / (double)(info.y-1) * (double)y; //長さ
					double rl = abs(l - r);			//中心からの距離
					double u = info.velocity->get(0)*(1.0 - (rl*rl) / (r*r));
					p->velocity.set(u, 0, 0);
					/*double l = abs(info.cld/2.0 - (double)y/(double)(info.y-1) * info.cld);//中心からの距離
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
	//圧力、速度の計算
	for (int a = 0; a < info.directionNum; a++) {
		//巨視的密度の計算
		p->density += p->distribut[a];
		//巨視的速度の計算
		temp_e.copy(&e[a]);
		temp_e.mult(c, &temp_e);
		//e[a].mult(p->distribut[a], &temp);
		temp_e.mult(p->distribut[a], &temp);
		p->velocity.add(&temp);
	}
	//巨視的速度の計算
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
		//格子外を参照→境界条件で値を取得
		//return boundaryGet(x, y, z,a, type);
		return nullptr;
	}
	double _x = cx - x;
	double _y = cy - y;

	if (sqrt(_x*_x + _y*_y) <= r) {
		//円に入っている
		return nullptr;
	}
	/*else if (isObject(x, y, z)){
	//オブジェクト内
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
	//上下は周期境界
	if (_y < 0 || _y >= info.y) {
		//return getPoint(x, y, z, type)->distribut[a];
		_y = info.y - abs(_y);
	}
	if (_x < 0 || _x >= info.x) {
		//_x = info.x - abs(_x);
		if (_x < 0) {
			//一様流入
			return inflow.distribut[a];
		}
		else {
			//流出
			return getPoint(x, _y, z, type)->distribut[a];
		}
	}
	double __x = cx - _x;
	double __y = cy - _y;

	if (sqrt(__x*__x + __y*__y) <= r) {
		//円に入っている
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
	}//上下はbounce-back
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
