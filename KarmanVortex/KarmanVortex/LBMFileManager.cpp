#include "LBMFileManager.h"



CLBMFileManager::CLBMFileManager()
{
}


CLBMFileManager::~CLBMFileManager()
{
}

CGnuplotFileManager::FILE CLBMFileManager::writeColorMap(string * fileName, CLBM* lbm, CLBM::LBMInfo info,CLBMFileManager::TYPE type) const
{
	int velocity_writeRate = 16;
	ofstream* ofs = nullptr;
	if (openFile(fileName, NEW, &ofs) != OK) {
		OutputDebugString("File Open Failured");
		return NOTOPEN;
	}
	for (int x = 1; x <= info.x;x++) {
		for (int y = 1; y <= info.y;y++) {
			if (type == TYPE::PRESSURE) {
				(*ofs) << info.deltaLength*x << " " << info.deltaLength*y << " " << lbm->getPoint(x - 1, y - 1, 0, CLBM::ACCESS::NOW)->density << std::endl;
				if ((y + 1) > info.y)
					(*ofs) << endl;
			}
			else if(x % velocity_writeRate == 0 && y % velocity_writeRate == 0){
				(*ofs) << 
					info.deltaLength*x << " " << 
					info.deltaLength*y << " " << 
					lbm->getPoint(x-1, y-1, 0, CLBM::ACCESS::NOW)->velocity.get(CVector3<int>::Dim::X) << " " <<
					lbm->getPoint(x-1, y-1, 0, CLBM::ACCESS::NOW)->velocity.get(CVector3<int>::Dim::Y) <<
					std::endl;
			}
		}
	}
}

CGnuplotFileManager::FILE CLBMFileManager::writeVelocityDistribution(string * fileName, CLBM * lbm, CLBM::LBMInfo* info) const
{
	ofstream* ofs = nullptr;
	if (openFile(fileName, MODE::NEW, &ofs) != FILE::OK) {
		OutputDebugString("error can't write data");
		return FILE::NOTOPEN;
	}
	double max = 0;
	//Å‘å’l‚ðŽæ“¾
	for (int n = 0; n < info->y; n++) {
		CLBM::Point *p = lbm->getPoint(info->x / 2, n, 0, CLBM::ACCESS::NOW);
		max = std::fmax(max, p->velocity.get(0));
	}
	int segment = 128;
	for (int n = 0; n < segment; n++) {
		double seg = ((double)info->y * ((double)n / (double)segment));
		CLBM::Point *p = lbm->getPoint(info->x / 2, seg, 0, CLBM::ACCESS::NOW);
		double data = p->velocity.get(0) / max;
		double y = ((double)info->y * ((double)n / (double)segment)) / (double)info->y;
		(*ofs) << p->velocity.get(0)/max << " "<< y << endl;
	}
	CLBM::Point *p = lbm->getPoint(info->x / 2,info->y - 1, 0, CLBM::ACCESS::NOW);
	(*ofs) << p->velocity.get(0) / max << " " << 1 << endl;
	ofs->close();
	return FILE::OK;
}

CGnuplotFileManager::FILE CLBMFileManager::writeVelocity(string * fileName, CLBM * lbm, CLBM::LBMInfo * info) const
{
	int write_rate_x = 4;
	int write_rate_y = 4;
	ofstream* ofs = nullptr;
	if (openFile(fileName, MODE::NEW, &ofs) != FILE::OK) {
		OutputDebugString("error can't write data");
		return FILE::NOTOPEN;
	}
	double aspect = (double)info->x / (double)info->y;
	for (int y = 0; y < info->y; y+=write_rate_y) {
		for (int x = 0; x < info->x; x+=write_rate_x) {
			CLBM::Point* point = lbm->getPoint(x, y, 0, CLBM::ACCESS::NOW);
			double _y = info->deltaLength * (double)y;
			double _x = info->deltaLength * (double)x;
			if (point == nullptr) {
				(*ofs) << _x << " " << _y << " " << 0 << " "<< 0 << endl;
			}
			else {
				(*ofs) << _x << " " << _y << " " << point->velocity.get(0) << " " << point->velocity.get(1) << endl;
			}
		}
		(*ofs) << endl;
	}
	ofs->close();
	return FILE::OK;
}

CGnuplotFileManager::FILE CLBMFileManager::writePresser(string * fileName, CLBM * lbm, CLBM::LBMInfo * info) const
{
	int write_rate_x = 4;
	int write_rate_y = 4;
	ofstream* ofs = nullptr;
	if (openFile(fileName, MODE::NEW, &ofs) != FILE::OK) {
		OutputDebugString("error can't write data");
		return FILE::NOTOPEN;
	}
	double aspect = (double)info->x / (double)info->y;
	for (int y = 0; y < info->y; y += write_rate_y) {
		for (int x = 0; x < info->x; x += write_rate_x) {
			CLBM::Point* point = lbm->getPoint(x, y, 0, CLBM::ACCESS::NOW);
			double _y = info->deltaLength * (double)y;
			double _x = info->deltaLength * (double)x;
			if (point == nullptr) {
				(*ofs) << _x << " " << _y << " " << 0 << endl;
			}
			else {
				(*ofs) << _x << " " << _y << " " << lbm->calcPresser(point) << endl;
			}
		}
		(*ofs) << endl;
	}
	ofs->close();
	return FILE::OK;
}
