#pragma once
#include "LBM.h"
#include "LBMFileManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <future>

#ifdef _OPENMP
#include <omp.h>
#endif

void lbmThread();

volatile bool stop;
volatile char command;
std::mutex mtx;

void dispPoint(CLBM::Point* point){
	printf("dist\n");
	for (int n = 0; n < 9; n++) {
		printf("\t%d:%f\n", n, point->distribut[n]);
	}
	printf("density:%f\n",point->density);
	printf("vect:(%f,%f,%f)]\n",point->velocity.get(0), point->velocity.get(1), point->velocity.get(2));
}

void main() {

	stop = false;

	auto thread = std::thread([] {lbmThread(); });
	
	char buff;
	while((buff = getchar()) != 'e'){
		if (buff == 's') {
			mtx.lock();
			command = buff;
			mtx.unlock();
		}
	}

	mtx.lock();
	stop = true;
	mtx.unlock();

	thread.join();
}

void lbmThread() {
	CLBMFileManager fm;

	CVector3<double> vel;
	vel.set(0.2, 0, 0);

	/*---------------------------*/
	//		単位に注意!!
	/*---------------------------*/
	int latticle_num = 16;	//代表格子点数
	CLBM::LBMInfo info;
	info.x = 512;
	info.y = 256;
	info.z = 1;
	info.directionNum = 9;
	info.cld = 0.01;	//1[m]
	info.cv = vel.get(0);	//1 [m/s]
	info.deltaLength = info.cld / (double)latticle_num;
	info.deltaTime = info.deltaLength;
	info.velocity = &vel;
	info.lambda = 68e-9;
	info.M = 0.028966;
	info.R = 8.3144598;
	info.T = 293.15;
	info.mu = 1.83e-5;
	info.density = (101325 * info.M)/(info.R * info.T);
	CLBM lbm(info);

	int writeRate = 1;

	printf("初期化終了\nレイノルズ数:%f\n",lbm.Re);
	int index = 0;
	bool _stop;
	char _command;
	int text_index = 0;
	int step = 0;
	while (true)
	{	
		/*排他制御*/
		mtx.lock();
		_stop = stop;
		_command = command;
		command = 'n';
		mtx.unlock();

		if (_stop)
			break;
		/*排他制御*/

		if (step % 10 == 0) {
			//fm.writeColorMap(&press[index].str(), &lbm, info, CLBMFileManager::TYPE::PRESSURE);
			//fm.writeColorMap(&velocity[text_index].str(), &lbm, info, CLBMFileManager::TYPE::VELOCITY);
			string fileName = "re_102_512_256_omp_v_2_" + std::to_string(text_index) + ".dat";
			string fileName_press = "re_102_512_256_omp_p_2_" + std::to_string(text_index) + ".dat";
			//fm.writeVelocityDistribution(&fileName, &lbm, &info);
			#pragma omp parallel
			{
				#pragma omp sections
				{
					#pragma omp section
					{
						fm.writeVelocity(&fileName, &lbm, &info);
					}
					#pragma omp section
					{
						fm.writePresser(&fileName_press, &lbm, &info);
					}
				}
			}
			printf("セーブしました\n");
			text_index++;
		}
		for (int n = 0; n < info.y; n += (info.y/15)) {
			CLBM::Point* p = lbm.getPoint(info.x / 2, n, 0, CLBM::ACCESS::NOW);
			printf("%3d : %g , %g\n", n, p->velocity.get(0), lbm.getPoint(info.x / 2, info.y - 1, 0, CLBM::ACCESS::NOW)->velocity.get(1));
		}
		printf("%3d : %g , %g\n", info.y-1, lbm.getPoint(info.x / 2, info.y-1, 0, CLBM::ACCESS::NOW)->velocity.get(0), lbm.getPoint(info.x / 2, info.y - 1, 0, CLBM::ACCESS::NOW)->velocity.get(1));
		printf("%dステップ終了\n",index);
		index++;
		//}
		lbm.calcStep();
		step++;
	}

	printf("終了します\n");
}