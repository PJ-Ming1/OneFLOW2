/*---------------------------------------------------------------------------*\
	OneFLOW - LargeScale Multiphysics Scientific Simulation Environment
	Copyright (C) 2017-2019 He Xin and the OneFLOW contributors.
-------------------------------------------------------------------------------
License
	This file is part of OneFLOW.

	OneFLOW is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	OneFLOW is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
	for more details.

	You should have received a copy of the GNU General Public License
	along with OneFLOW.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "UINsInvterm.h"
#include "INsInvterm.h"
#include "UINsVisterm.h"
#include "UINsGrad.h"
#include "UGrad.h"
#include "BcData.h"
#include "Zone.h"
#include "Atmosphere.h"
#include "UnsGrid.h"
#include "DataBase.h"
#include "UCom.h"
#include "UINsCom.h"
#include "INsCom.h"
#include "INsIdx.h"
#include "HXMath.h"
#include "Multigrid.h"
#include "Boundary.h"
#include "BcRecord.h"
#include "UINsLimiter.h"
#include "FieldImp.h"
#include "Iteration.h"
#include "TurbCom.h"
#include "UTurbCom.h"
#include "Ctrl.h"
#include <iostream>
#include <iomanip>
using namespace std;

BeginNameSpace(ONEFLOW)

UINsInvterm::UINsInvterm()
{
	limiter = new INsLimiter();
	limf = limiter->limf;
}

UINsInvterm::~UINsInvterm()
{
	delete limiter;
}

void UINsInvterm::CmpLimiter()
{
	limiter->CmpLimiter();
}

void UINsInvterm::CmpInvFace()  //单元数据重构
{
	this->CmpLimiter();   //不改

	this->GetQlQrField();  //不改

	//this->BoundaryQlQrFixField();  //不改，ghost单元
}

void UINsInvterm::GetQlQrField()
{
	limf->GetQlQr();
}

void UINsInvterm::ReconstructFaceValueField()
{
	limf->CmpFaceValue();
	//limf->CmpFaceValueWeighted();
}

void UINsInvterm::BoundaryQlQrFixField()
{
	limf->BcQlQrFix();
}

void UINsInvterm::CmpInvcoff()
{
	if (inscom.icmpInv == 0) return;

	this->CmpInvMassFlux(); 

}

void UINsInvterm::CmpINsTimestep()
{
	iinv.timestep = GetDataValue< Real >("global_dt");
}

void UINsInvterm::CmpINsPreflux()
{
		iinv.Init();
		ug.Init();
		uinsf.Init();
        this->Init();

		for (int ir = 0; ir < ug.nRegion; ++ir)
		{
			ug.ir = ir;
			ug.bctype = ug.bcRecord->bcInfo->bcType[ir];
			ug.nRBFace = ug.bcRecord->bcInfo->bcFace[ir].size();

			for (int ibc = 0; ibc < ug.nRBFace; ++ibc)
			{
				ug.bcfId = ibc;

				BcInfo* bcInfo = ug.bcRecord->bcInfo;

				int fId = bcInfo->bcFace[ug.ir][ibc];  
				ug.bcr = bcInfo->bcRegion[ug.ir][ibc];
				ug.bcdtkey = bcInfo->bcdtkey[ug.ir][ibc];

				if (ug.bcr == -1) return; //interface
				int dd = bcdata.r2d[ug.bcr];
				if (dd != -1)
				{
					ug.bcdtkey = 1;
					inscom.bcflow = &bcdata.dataList[dd];
				}

				INsExtractl(*uinsf.q, iinv.rl, iinv.ul, iinv.vl, iinv.wl, iinv.pl);

				if (ug.bctype == BC::SOLID_SURFACE)
				{
					if (ug.bcdtkey == 0)     
					{
						iinv.rf = iinv.rl;   

						iinv.uf[fId] = (*ug.vfx)[fId];

						iinv.vf[fId] = (*ug.vfy)[fId];

						iinv.wf[fId] = (*ug.vfz)[fId];

						iinv.pf[fId] = iinv.pl;

						iinv.fq[fId] = iinv.rf * ((*ug.a1)[fId] * iinv.uf[fId] + (*ug.a2)[fId] * iinv.vf[fId] + (*ug.a3)[fId] * iinv.wf[fId] - gcom.vfn);

					}
					else
					{
						iinv.rf = iinv.rl;   

						iinv.uf[fId] = (*inscom.bcflow)[1];

						iinv.vf[fId] = (*inscom.bcflow)[2];

						iinv.wf[fId] = (*inscom.bcflow)[3];

						iinv.pf[fId] = iinv.pl;

						iinv.fq[fId] = iinv.rf * ((*ug.a1)[fId] * iinv.uf[fId] + (*ug.a2)[fId] * iinv.vf[fId] + (*ug.a3)[fId] * iinv.wf[fId] - gcom.vfn);

					}

				}
				else if (ug.bctype == BC::OUTFLOW)
				{

					iinv.rf = iinv.rl;   

					iinv.uf[fId] = iinv.ul;

					iinv.vf[fId] = iinv.vl;

					iinv.wf[fId] = iinv.wl;

					iinv.pf[fId] = iinv.pl;

					iinv.fq[fId] = iinv.rf * ((*ug.a1)[fId] * iinv.uf[fId] + (*ug.a2)[fId] * iinv.vf[fId] + (*ug.a3)[fId] * iinv.wf[fId] - gcom.vfn);

				}

				else if (ug.bctype == BC::INFLOW)
				{
					iinv.rf = inscom.inflow[0];   

					iinv.uf[fId] = inscom.inflow[1];

					iinv.vf[fId] = inscom.inflow[2];

					iinv.wf[fId] = inscom.inflow[3];

					iinv.pf[fId] = inscom.inflow[4];

					iinv.fq[fId] = iinv.rf * ((*ug.a1)[fId] * iinv.uf[fId] + (*ug.a2)[fId] * iinv.vf[fId] + (*ug.a3)[fId] * iinv.wf[fId] - gcom.vfn);
				}
			}
		}

		for (int fId = ug.nBFace; fId < ug.nFace; fId++)
		{
			ug.lc = (*ug.lcf)[fId];
			ug.rc = (*ug.rcf)[fId];

			INsExtractl(*uinsf.q, iinv.rl, iinv.ul, iinv.vl, iinv.wl, iinv.pl);
			INsExtractr(*uinsf.q, iinv.rr, iinv.ur, iinv.vr, iinv.wr, iinv.pr);

			iinv.rf = iinv.rl * (*ug.fl)[fId] + iinv.rr * (*ug.fr)[fId];

			iinv.uf[fId] = iinv.ul * (*ug.fl)[fId] + iinv.ur * (*ug.fr)[fId];

			iinv.vf[fId] = iinv.vl * (*ug.fl)[fId] + iinv.vr * (*ug.fr)[fId];

			iinv.wf[fId] = iinv.wl * (*ug.fl)[fId] + iinv.wr * (*ug.fr)[fId];

			iinv.pf[fId] = iinv.pl * (*ug.fl)[fId] + iinv.pr * (*ug.fr)[fId];

			iinv.fq[fId] = iinv.rf * ((*ug.a1)[fId] * iinv.uf[fId] + (*ug.a2)[fId] * iinv.vf[fId] + (*ug.a3)[fId] * iinv.wf[fId] - gcom.vfn);
		}


		/*RealField massflux = 0;
		massflux.resize(ug.nCell);
		for (int cId = 0; cId < ug.nCell; cId++)
		{
			int fn = (*ug.c2f)[cId].size();
			for (int iFace = 0; iFace < fn; iFace++)
			{
				int fId = (*ug.c2f)[cId][iFace];
				massflux[cId] += iinv.fq[fId];
			}
			std::cout << "cId: " << cId << ", massflux[cId]: " << massflux[cId] << std::endl;
		}*/
}

void UINsInvterm::Init()
{
	//iinv.f1.resize(ug.nFace);
	//iinv.f2.resize(ug.nFace);
	
	iinv.uf.resize(ug.nFace);
	iinv.vf.resize(ug.nFace);
	iinv.wf.resize(ug.nFace);
	iinv.Vdvu.resize(ug.nFace);
	iinv.Vdvv.resize(ug.nFace);
	iinv.Vdvw.resize(ug.nFace);
	iinv.aju.resize(ug.nFace);
	iinv.ajv.resize(ug.nFace);
	iinv.ajw.resize(ug.nFace);
	iinv.VdU.resize(ug.nTCell);
	iinv.VdV.resize(ug.nTCell);
	iinv.VdW.resize(ug.nTCell);
	iinv.buc.resize(ug.nTCell);
	iinv.bvc.resize(ug.nTCell);
	iinv.bwc.resize(ug.nTCell);
	iinv.bp.resize(ug.nTCell);
	//iinv.sju.resize(ug.nTCell);
	//iinv.sjv.resize(ug.nTCell);
	//iinv.sjw.resize(ug.nTCell);
	iinv.fq.resize(ug.nFace);
	iinv.duf.resize(ug.nFace);
	iinv.spc.resize(ug.nTCell);
	iinv.ai.resize(ug.nFace, 2);
	//iinv.biu.resize(ug.nFace,2);
	//iinv.biv.resize(ug.nFace,2);
	//iinv.biw.resize(ug.nFace,2);
	//iinv.sj.resize(ug.nTCell, 4);
	//iinv.sd.resize(ug.nTCell, 4);
	//iinv.sjp.resize(ug.nCell, ug.nCell);
	//iinv.sjd.resize(ug.nCell, ug.nCell);
	iinv.spp.resize(ug.nTCell);
	iinv.pp.resize(ug.nTCell);
	iinv.uu.resize(ug.nTCell);
	iinv.vv.resize(ug.nTCell);
	iinv.ww.resize(ug.nTCell);
	iinv.uuj.resize(ug.nFace);
	iinv.vvj.resize(ug.nFace);
	iinv.wwj.resize(ug.nFace);
	iinv.muc.resize(ug.nTCell);
	iinv.mvc.resize(ug.nTCell);
	iinv.mwc.resize(ug.nTCell);
	iinv.mp.resize(ug.nTCell);

	iinv.dqqdx.resize(ug.nTCell);
	iinv.dqqdy.resize(ug.nTCell);
	iinv.dqqdz.resize(ug.nTCell);
	//iinv.Fnu.resize(ug.nFace);
	//iinv.Fnv.resize(ug.nFace);
	//iinv.Fnw.resize(ug.nFace);
	//iinv.Fpu.resize(ug.nFace);
	//iinv.Fpv.resize(ug.nFace);
	//iinv.Fpw.resize(ug.nFace);
	//iinv.dsrl.resize(ug.nFace);
	//iinv.elrn.resize(ug.nFace);
	//iinv.value.resize(ug.nFace);
	iinv.mu.resize(ug.nCell);
	iinv.mv.resize(ug.nCell);
	iinv.mw.resize(ug.nCell);
	iinv.mua.resize(ug.nCell);
	iinv.mva.resize(ug.nCell);
	iinv.mwa.resize(ug.nCell);
	iinv.res_pp.resize(ug.nCell);
	iinv.res_up.resize(ug.nCell);
	iinv.res_vp.resize(ug.nCell);
	iinv.res_wp.resize(ug.nCell);
	iinv.up.resize(ug.nCell);
	iinv.vp.resize(ug.nCell);
	iinv.wp.resize(ug.nCell);
	//iinv.op.resize(ug.nBFace);
	//iinv.dj.resize(ug.nCell);
	iinv.pf.resize(ug.nFace);
	iinv.ppf.resize(ug.nFace);
	iinv.uuf.resize(ug.nBFace);
	iinv.vvf.resize(ug.nBFace);
	iinv.wwf.resize(ug.nBFace);


	iinv.ai1 = 0;
	iinv.ai2 = 0;


	iinv.buc = 0;
	iinv.bvc = 0;
	iinv.bwc = 0;

	iinv.spj = 0;
	iinv.spp = 0;
	iinv.sppu = 0;
	iinv.sppv = 0;
	iinv.sppw = 0;

	iinv.bpu = 0;
	iinv.bpv = 0;
	iinv.bpw = 0;
	iinv.bp = 0;
	iinv.pp = 0;

	iinv.muc = 0;
	iinv.mvc = 0;
	iinv.mwc = 0;

	iinv.uu = 0;
	iinv.vv = 0;
	iinv.ww = 0;
}

void UINsInvterm::InitInv()
{
	iinv.spc = 0;
	iinv.buc = 0;
	iinv.bvc= 0;
	iinv.bwc= 0;
}

void UINsInvterm::CmpInvMassFlux()
{
	InitInv();

	for(int fId = 0; fId < ug.nBFace; fId++)
	{
		ug.fId = fId;
		ug.lc = (*ug.lcf)[fId];
		ug.rc = (*ug.rcf)[fId];

		this->CmpINsBcinvTerm();
		
	}

	for (int fId = ug.nBFace; fId < ug.nFace; ++fId)
	{
		ug.fId = fId;

		this->CmpINsinvTerm();
	}

}

void UINsInvterm::PrepareFaceValue()
{
	gcom.xfn = (*ug.xfn)[ug.fId];
	gcom.yfn = (*ug.yfn)[ug.fId];
	gcom.zfn = (*ug.zfn)[ug.fId];
	gcom.vfn = (*ug.vfn)[ug.fId];
	gcom.farea = (*ug.farea)[ug.fId];

	//inscom.gama1 = (*uinsf.gama)[0][ug.lc];
	//inscom.gama2 = (*uinsf.gama)[0][ug.rc];

	//iinv.gama1 = inscom.gama1;
	//iinv.gama2 = inscom.gama2;

	for (int iEqu = 0; iEqu < limf->nEqu; ++iEqu)
	{
		iinv.prim1[iEqu] = (*limf->q)[iEqu][ug.lc];         
		iinv.prim2[iEqu] = (*limf->q)[iEqu][ug.rc];
	}
}

void UINsInvterm::PrepareProFaceValue()
{
	gcom.xfn = (*ug.xfn)[ug.fId];
	gcom.yfn = (*ug.yfn)[ug.fId];
	gcom.zfn = (*ug.zfn)[ug.fId];
	gcom.vfn = (*ug.vfn)[ug.fId];
	gcom.farea = (*ug.farea)[ug.fId];

	/*iinv.prim1[IIDX::IIR] = (*uinsf.q)[IIDX::IIR][ug.lc];
	iinv.prim1[IIDX::IIU] = iinv.uc[ug.lc];
	iinv.prim1[IIDX::IIV] = iinv.vc[ug.lc];
	iinv.prim1[IIDX::IIW] = iinv.wc[ug.lc];
	iinv.prim1[IIDX::IIP] = (*uinsf.q)[IIDX::IIP][ug.lc];

	iinv.prim2[IIDX::IIR] = (*uinsf.q)[IIDX::IIR][ug.rc];
	iinv.prim2[IIDX::IIU] = iinv.uc[ug.rc];
	iinv.prim2[IIDX::IIV] = iinv.vc[ug.rc];
	iinv.prim2[IIDX::IIW] = iinv.wc[ug.rc];
	iinv.prim2[IIDX::IIP] = (*uinsf.q)[IIDX::IIP][ug.rc];*/

}

UINsInvterm NonZero;

void UINsInvterm::MomPre()
{
	this->CmpINsMomRes();

	//BGMRES求解
	NonZero.Number = 0;
	RealField dj;
	dj.resize(ug.nCell);
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		dj[cId] = (*ug.c2f)[cId].size();
		for (int iFace = 0; iFace < (*ug.c2f)[cId].size(); ++iFace)
		{
			int fId = (*ug.c2f)[cId][iFace];
			if (fId < ug.nBFace)
			{
				dj[cId] -= 1;
			}
		}
		Rank.NUMBER += dj[cId];
	}
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		int fn = (*ug.c2f)[cId].size();                             //相邻单元的个数                                    
		NonZero.Number += fn;                                          //非对角线上非零元的个数
	}
	NonZero.Number = NonZero.Number + ug.nCell;                     //非零元的总个数         
	Rank.RANKNUMBER = ug.nCell;                                     // 矩阵的行列大小
	Rank.NUMBER = NonZero.Number;                                    // 矩阵非零元素个数传到计算程序中
	Rank.COLNUMBER = 1;                                              //右端项个数
	Rank.Init();                                                     //传入GMRES计算程序的中间变量
	double residual_u, residual_v, residual_w;
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		Rank.TempIA[0] = 0;
		int n = Rank.TempIA[cId];
		int fn = (*ug.c2f)[cId].size();
		Rank.TempIA[cId + 1] = Rank.TempIA[cId] + dj[cId] + 1;                                                  // 前n+1行非零元素的个数
		for (int iFace = 0; iFace < fn; ++iFace)
		{
			int fId = (*ug.c2f)[cId][iFace];                                                            // 相邻面的编号
			ug.lc = (*ug.lcf)[fId];                                                                     // 面左侧单元
			ug.rc = (*ug.rcf)[fId];                                                                     // 面右侧单元

			if (fId > ug.nBFace - 1)
			{
				if (cId == ug.lc)
				{
					Rank.TempA[n + iFace] = -iinv.ai[fId][0];
					Rank.TempJA[n + iFace] = ug.rc;
				}
				else if (cId == ug.rc)
				{
					Rank.TempA[n + iFace] = -iinv.ai[fId][1];
					Rank.TempJA[n + iFace] = ug.lc;
				}
			}
			else
			{
				continue;
			}
		}

		int fj = dj[cId];
		Rank.TempA[n + fj] = iinv.spc[cId];                          //主对角线元素值
		Rank.TempJA[n + fj] = cId;                                      //主对角线纵坐标

	}
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		Rank.TempB[cId][0] = iinv.buc[cId];
	}
	bgx.BGMRES();
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		(*uinsf.q)[IIDX::IIU][cId] += Rank.TempX[cId][0];                       // 解的输出
	}
	residual_u = Rank.residual;
	iinv.res_u = residual_u;

	Rank.Deallocate();
	//cout << "residual_u:" << residual_u << endl;


	NonZero.Number = 0;
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		int fn = (*ug.c2f)[cId].size();                             //相邻单元的个数                                    
		NonZero.Number += fn;                                          //非对角线上非零元的个数
	}
	NonZero.Number = NonZero.Number + ug.nCell;                     //非零元的总个数         
	Rank.RANKNUMBER = ug.nCell;                                     // 矩阵的行列大小
	Rank.NUMBER = NonZero.Number;                                    // 矩阵非零元素个数传到计算程序中
	Rank.COLNUMBER = 1;                                              //右端项个数
	Rank.Init();                                                     //传入GMRES计算程序的中间变量
	//double residual_u, residual_v, residual_w;
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		Rank.TempIA[0] = 0;
		int n = Rank.TempIA[cId];
		int fn = (*ug.c2f)[cId].size();
		Rank.TempIA[cId + 1] = Rank.TempIA[cId] + dj[cId] + 1;                                                  // 前n+1行非零元素的个数
		for (int iFace = 0; iFace < fn; ++iFace)
		{
			int fId = (*ug.c2f)[cId][iFace];                                                            // 相邻面的编号
			ug.lc = (*ug.lcf)[fId];                                                                     // 面左侧单元
			ug.rc = (*ug.rcf)[fId];                                                                     // 面右侧单元

			if (fId > ug.nBFace - 1)
			{
				if (cId == ug.lc)
				{
					Rank.TempA[n + iFace] = -iinv.ai[fId][0];
					Rank.TempJA[n + iFace] = ug.rc;
				}
				else if (cId == ug.rc)
				{
					Rank.TempA[n + iFace] = -iinv.ai[fId][1];
					Rank.TempJA[n + iFace] = ug.lc;
				}
			}
			else
			{
				continue;
			}
		}
		int fj = dj[cId];
		Rank.TempA[n + fj] = iinv.spc[cId];                          //主对角线元素值
		Rank.TempJA[n + fj] = cId;                                      //主对角线纵坐标

	}


	for (int cId = 0; cId < ug.nCell; cId++)
	{
		Rank.TempB[cId][0] = iinv.bvc[cId];
	}
	bgx.BGMRES();
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		(*uinsf.q)[IIDX::IIV][cId] += Rank.TempX[cId][0];
	}
	residual_v = Rank.residual;
	iinv.res_v = residual_v;

	Rank.Deallocate();

	//cout << "residual_v:" << residual_v << endl;


	NonZero.Number = 0;
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		int fn = (*ug.c2f)[cId].size();                             //相邻单元的个数                                    
		NonZero.Number += fn;                                          //非对角线上非零元的个数
	}
	NonZero.Number = NonZero.Number + ug.nCell;                     //非零元的总个数         
	Rank.RANKNUMBER = ug.nCell;                                     // 矩阵的行列大小
	Rank.NUMBER = NonZero.Number;                                    // 矩阵非零元素个数传到计算程序中
	Rank.COLNUMBER = 1;                                              //右端项个数
	Rank.Init();                                                     //传入GMRES计算程序的中间变量
	//double residual_u, residual_v, residual_w;
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		Rank.TempIA[0] = 0;
		int n = Rank.TempIA[cId];
		int fn = (*ug.c2f)[cId].size();
		Rank.TempIA[cId + 1] = Rank.TempIA[cId] + dj[cId] + 1;                                                  // 前n+1行非零元素的个数
		for (int iFace = 0; iFace < fn; ++iFace)
		{
			int fId = (*ug.c2f)[cId][iFace];                                                            // 相邻面的编号
			ug.lc = (*ug.lcf)[fId];                                                                     // 面左侧单元
			ug.rc = (*ug.rcf)[fId];                                                                     // 面右侧单元

			if (fId > ug.nBFace - 1)
			{
				if (cId == ug.lc)
				{
					Rank.TempA[n + iFace] = -iinv.ai[fId][0];
					Rank.TempJA[n + iFace] = ug.rc;
				}
				else if (cId == ug.rc)
				{
					Rank.TempA[n + iFace] = -iinv.ai[fId][1];
					Rank.TempJA[n + iFace] = ug.lc;
				}
			}
			else
			{
				continue;
			}

		}
		int fj = dj[cId];
		Rank.TempA[n + fj] = iinv.spc[cId];                          //主对角线元素值
		Rank.TempJA[n + fj] = cId;                                      //主对角线纵坐标

	}

	for (int cId = 0; cId < ug.nCell; cId++)
	{
		Rank.TempB[cId][0] = iinv.bwc[cId];
	}
	bgx.BGMRES();
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		(*uinsf.q)[IIDX::IIW][cId] += Rank.TempX[cId][0];
	}
	residual_w = Rank.residual;
	iinv.res_w = residual_w;

	Rank.Deallocate();

	//cout << "residual_w:" << residual_w << endl;


	/*RealField uCorrect, vCorrect, wCorrect;
	uCorrect.resize(ug.nCell);
	vCorrect.resize(ug.nCell);
	wCorrect.resize(ug.nCell);
	this->CmpINsMomRes();
	this->SolveEquation(iinv.spc, iinv.ai, iinv.buc, uCorrect, iinv.res_u);
	this->SolveEquation(iinv.spc, iinv.ai, iinv.bvc, vCorrect, iinv.res_v);
	this->SolveEquation(iinv.spc, iinv.ai, iinv.bwc, wCorrect, iinv.res_w);
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		(*uinsf.q)[IIDX::IIU][cId] += uCorrect[cId];
		(*uinsf.q)[IIDX::IIV][cId] += vCorrect[cId];
		(*uinsf.q)[IIDX::IIW][cId] += wCorrect[cId];
	}*/
}

void UINsInvterm::SolveEquation(RealField& sp, RealField2D& ai, RealField& b, RealField& x, Real res)
{

	Rank.NUMBER = 0;
	Rank.RANKNUMBER = ug.nCell;
	Rank.COLNUMBER = 1;

	RealField dj;
	dj.resize(ug.nCell);
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		dj[cId] = (*ug.c2f)[cId].size();
		for (int iFace = 0; iFace < (*ug.c2f)[cId].size(); ++iFace)
		{
			int fId = (*ug.c2f)[cId][iFace];
			if (fId < ug.nBFace)
			{
				dj[cId] -= 1;
			}
		}
		Rank.NUMBER += dj[cId];
	}
	Rank.NUMBER += ug.nCell;
	Rank.Init();

	//ofstream file("CoeMatrix.txt", ios::app);
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		Rank.TempIA[0] = 0;
		int n = Rank.TempIA[cId];
		int fn = (*ug.c2f)[cId].size();
		Rank.TempIA[cId + 1] = Rank.TempIA[cId] + dj[cId] + 1;
		int tempCout = 0;
		for (int iFace = 0; iFace < fn; ++iFace)
		{
			int fId = (*ug.c2f)[cId][iFace];
			int lc = (*ug.lcf)[fId];

			if (fId > ug.nBFace - 1)
			{
				int rc = (*ug.rcf)[fId];
				if (cId == lc)
				{
					Rank.TempA[n + tempCout] = -ai[fId][0];
					Rank.TempJA[n + tempCout] = rc;
					//file << cId + 1 << "\t" << rc + 1 << "\t" << setprecision(18) << Rank.TempA[n + tempCout] << std::endl;
					tempCout += 1;
				}
				else if (cId == rc)
				{
					Rank.TempA[n + tempCout] = -ai[fId][1];
					Rank.TempJA[n + tempCout] = lc;
					//file << cId + 1 << "\t" << lc + 1 << "\t" << setprecision(18) << Rank.TempA[n + tempCout] << std::endl;
					tempCout += 1;
				}
			}
			else
			{
				continue;
			}
		}

		int fj = dj[cId];
		Rank.TempA[n + fj] = sp[cId];
		Rank.TempJA[n + fj] = cId;
		//file << cId + 1 << "\t" << cId + 1 << "\t" << setprecision(18) << sp[cId] << std::endl;
	}
	//file.close();
	//ofstream RHS("rhs.txt", ios::app);
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		Rank.TempB[cId][0] = b[cId];
		//RHS << setprecision(18) << Rank.TempB[cId][0] << endl;
	}
	//RHS.close();
	bgx.BGMRES();
	//ofstream X("x.txt", ios::app);
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		x[cId] = Rank.TempX[cId][0];
		//X << x[cId] << endl;
	}
	//X.close();
	res = Rank.residual;

	Rank.Deallocate();

}

void UINsInvterm::CmpFaceflux()
{
	RealField dpdx, dpdy, dpdz;
	dpdx.resize(ug.nCell);
	dpdy.resize(ug.nCell);
	dpdz.resize(ug.nCell);
	ONEFLOW::CmpINsGrad(iinv.pf, dpdx, dpdy, dpdz);

	for (int fId = ug.nBFace; fId < ug.nFace; ++fId)
	{
		ug.fId = fId;
		ug.lc = (*ug.lcf)[ug.fId];
		ug.rc = (*ug.rcf)[ug.fId];
		
		CmpINsFaceflux(dpdx, dpdy, dpdz);
	}

	ug.nRegion = ug.bcRecord->bcInfo->bcType.size();
	BcInfo* bcInfo = ug.bcRecord->bcInfo;

	for (int ir = 0; ir < ug.nRegion; ++ir)
	{
		ug.ir = ir;
		ug.bctype = ug.bcRecord->bcInfo->bcType[ir];
		ug.nRBFace = ug.bcRecord->bcInfo->bcFace[ir].size();

		for (int ibc = 0; ibc < ug.nRBFace; ++ibc)
		{
			ug.lc = (*ug.lcf)[ug.fId];
			ug.rc = (*ug.rcf)[ug.fId];
			ug.bcfId = ibc;

			BcInfo* bcInfo = ug.bcRecord->bcInfo;

			ug.fId = bcInfo->bcFace[ug.ir][ibc];
			ug.bcr = bcInfo->bcRegion[ug.ir][ibc];
			ug.bcdtkey = bcInfo->bcdtkey[ug.ir][ibc];

			if (ug.bcr == -1) return; //interface
			int dd = bcdata.r2d[ug.bcr];
			if (dd != -1)
			{
				ug.bcdtkey = 1;
				inscom.bcflow = &bcdata.dataList[dd];
			}

			CmpINsBcFaceflux(dpdx, dpdy, dpdz);
		}
	}


	/*RealField massflux = 0;
	massflux.resize(ug.nCell);
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		int fn = (*ug.c2f)[cId].size();
		for (int iFace = 0; iFace < fn; iFace++)
		{
			int fId = (*ug.c2f)[cId][iFace];
			massflux[cId] += iinv.fq[fId];
		}
		std::cout << "cId: " << cId << ", massflux[cId]: " << massflux[cId] << std::endl;
	}*/


}

void UINsInvterm::CmpINsMomRes()
{
	iinv.res_u = 0;
	iinv.res_v = 0;
	iinv.res_w = 0;
}

void UINsInvterm::AddFlux()
{
	UnsGrid* grid = Zone::GetUnsGrid();
	MRField* res = GetFieldPointer< MRField >(grid, "res");
	int nEqu = res->GetNEqu();
	for (int fId = 0; fId < ug.nBFace; ++fId)
	{
		ug.fId = fId;
		ug.lc = (*ug.lcf)[ug.fId];
		ug.rc = (*ug.rcf)[ug.fId];
		//if ( ug.lc == 0 ) cout << fId << endl;

		for (int iEqu = 0; iEqu < nEqu; ++iEqu)
		{
			(*res)[iEqu][ug.lc] -= (*iinvflux)[iEqu][ug.fId];
		}
	}

	for (int fId = ug.nBFace; fId < ug.nFace; ++fId)
	{
		ug.fId = fId;
		ug.lc = (*ug.lcf)[ug.fId];
		ug.rc = (*ug.rcf)[ug.fId];

		//if ( ug.lc == 0 || ug.rc == 0 ) cout << fId << endl;

		for (int iEqu = 0; iEqu < nEqu; ++iEqu)
		{
			(*res)[iEqu][ug.lc] -= (*iinvflux)[iEqu][ug.fId];
			(*res)[iEqu][ug.rc] += (*iinvflux)[iEqu][ug.fId];
		}
	}

	//ONEFLOW::AddF2CField(res, iinvflux);

}

void UINsInvterm::InitPresscoef()
{
	iinv.spp = 0;
	iinv.bp = 0;
}

void UINsInvterm::CmpCorrectPresscoef()
{
	//this->CmpNewMomCoe();
	InitPresscoef();

	for (int fId = ug.nBFace; fId < ug.nFace; ++fId)
	{
		ug.fId = fId;

		ug.lc = (*ug.lcf)[ug.fId];
		ug.rc = (*ug.rcf)[ug.fId];

		this->CmpINsFaceCorrectPresscoef();
	}

	for (int fId = 0; fId < ug.nBFace; ++fId)
	{
		ug.fId = fId;

		ug.lc = (*ug.lcf)[ug.fId];
		ug.rc = (*ug.rcf)[ug.fId];

		this->CmpINsBcFaceCorrectPresscoef();
	}

	iinv.remax_pp = 0;
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		//iinv.remax_pp = MAX(abs(iinv.remax_pp), abs(iinv.bp[cId]));
		iinv.remax_pp += abs(iinv.bp[cId]);
	}
}

void UINsInvterm::maxmin(RealField& a, Real& max_a, Real& min_a)
{
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		max_a = MAX(max_a, a[cId]);
		min_a = MIN(min_a, a[cId]);
	}
}

void UINsInvterm::CmpPressCorrectEqu()
{
	//this->SolveEquation(iinv.spp, iinv.ai, iinv.bp, iinv.pp, iinv.res_p);
	
	//BGMRES求解
	NonZero.Number = 0;
	RealField dj;
	dj.resize(ug.nCell);
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		dj[cId] = (*ug.c2f)[cId].size();
		for (int iFace = 0; iFace < (*ug.c2f)[cId].size(); ++iFace)
		{
			int fId = (*ug.c2f)[cId][iFace];
			if (fId < ug.nBFace)
			{
				dj[cId] -= 1;
			}
		}
		Rank.NUMBER += dj[cId];
	}
	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		//ug.cId = cId;                                                                  // 主单元编号
		int fn = (*ug.c2f)[cId].size();                                                                 // 单元相邻面的个数
		//NonZero.Number += iinv.dj[cId];
		NonZero.Number += fn;
	}

	NonZero.Number = NonZero.Number + ug.nCell;                                                        // 非零元素的计数
	Rank.RANKNUMBER = ug.nCell;                                                                        // 矩阵的行列
	Rank.COLNUMBER = 1;
	Rank.NUMBER = NonZero.Number;                                                                      // 矩阵非零元素个数
	Rank.Init();
	double residual_p;

	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		iinv.ppd = iinv.pp[cId];
		Rank.TempIA[0] = 0;
		int n = Rank.TempIA[cId];
		int fn = (*ug.c2f)[cId].size();
		Rank.TempIA[cId + 1] = Rank.TempIA[cId] + dj[cId] + 1;                  // 前n+1行非零元素的个数
		for (int iFace = 0; iFace < fn; ++iFace)
		{
			int fId = (*ug.c2f)[cId][iFace];                           // 相邻面的编号
			ug.fId = fId;
			ug.lc = (*ug.lcf)[fId];                                    // 面左侧单元
			ug.rc = (*ug.rcf)[fId];                                    // 面右侧单元

			if (fId > ug.nBFace - 1)
			{
				if (cId == ug.lc)
				{
					Rank.TempA[n + iFace] = --iinv.ai[fId][0];          //非对角线元素值
					Rank.TempJA[n + iFace] = ug.rc;                           //非对角线元素纵坐标
				}
				else if (cId == ug.rc)
				{
					Rank.TempA[n + iFace] = --iinv.ai[fId][1];          //非对角线元素值
					Rank.TempJA[n + iFace] = ug.lc;                           //非对角线元素纵坐标
				}

			}
			else
			{
				continue;
			}
		}

		int fj = dj[cId];
		Rank.TempA[n + fj] = iinv.spp[cId];                            //主对角线元素
		Rank.TempJA[n + fj] = cId;                                        //主对角线纵坐标

		Rank.TempB[cId][0] = iinv.bp[cId];                             //右端项
	}
	bgx.BGMRES();
	residual_p = Rank.residual;
	//cout << "residual_p:" << residual_p << endl;
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		//ug.cId = cId;
		iinv.pp[cId] = Rank.TempX[cId][0]; //当前时刻的压力修正值
	}

	Rank.Deallocate();


	Real max_pp = 0;
	Real min_pp = 0;
	this->maxmin(iinv.pp, max_pp, min_pp);

	//边界单元
	for (int fId = 0; fId < ug.nBFace; ++fId)
	{
		int lc = (*ug.lcf)[fId];
		int rc = (*ug.rcf)[fId];

		int bcType = ug.bcRecord->bcType[fId];

		if (bcType == BC::OUTFLOW)
		{
			iinv.ppf[fId] = 0;
		}

		else if (ug.bctype == BC::SOLID_SURFACE)
		{
			iinv.ppf[fId] = iinv.pp[lc];
		}

		else if (ug.bctype == BC::INFLOW)
		{
			iinv.ppf[fId] = iinv.pp[lc];
		}

		else if (ug.bctype == BC::FIXP)
		{
			iinv.ppf[fId] = 0;
		}
	}

	for (int fId = ug.nBFace; fId < ug.nFace; ++fId)
	{
		int lc = (*ug.lcf)[fId];
		int rc = (*ug.rcf)[fId];

		iinv.ppf[fId] = (*ug.fl)[fId] * iinv.pp[lc] + (*ug.fr)[fId] * iinv.pp[rc];
	}

	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		(*uinsf.q)[IIDX::IIP][cId] = (*uinsf.q)[IIDX::IIP][cId] + 0.4 * iinv.pp[cId];
	}

	for (int fId = ug.nBFace; fId < ug.nFace; ++fId)
	{
		int lc = (*ug.lcf)[fId];
		int rc = (*ug.rcf)[fId];
		iinv.pf[fId] = (*ug.fl)[fId] * (*uinsf.q)[IIDX::IIP][lc] + (*ug.fr)[fId] * (*uinsf.q)[IIDX::IIP][rc];
	}

	for (int fId = 0; fId < ug.nBFace; fId++)
	{
		int bcType = ug.bcRecord->bcType[fId];
		int lc = (*ug.lcf)[fId];

		if (bcType == BC::SOLID_SURFACE)
		{
			iinv.pf[fId] = (*uinsf.q)[IIDX::IIP][lc];
			
		}
		else if (ug.bctype == BC::INFLOW)
		{
			iinv.pf[fId] = (*uinsf.q)[IIDX::IIP][lc];
		}
		else if (bcType == BC::OUTFLOW)
		{
			iinv.pf[fId] += 0;
		}
	}

	for (int fId = 0; fId < ug.nBFace; fId++)
	{
		int rc = (*ug.rcf)[fId];
		iinv.pf[fId] = iinv.pf[fId] + iinv.ppf[fId];
		(*uinsf.q)[IIDX::IIP][rc] = iinv.pf[fId];
	}
}


void UINsInvterm::CmpINsPreRes()
{
	//iinv.res_p = 0;


	for (int cId = 0; cId < ug.nTCell; ++cId)
	{
		ug.cId = cId;

		if (ug.cId == 0)
		{
			iinv.res_p = abs(iinv.bp[ug.cId]);
		}
		else
		{
			iinv.res_p = MAX(abs(iinv.bp[ug.cId]), abs(iinv.bp[ug.cId - 1]));
		}

		//iinv.res_p += (iinv.bp[ug.cId]+iinv.mp[ug.cId] - iinv.pp1[ug.cId]* (0.01+iinv.spp[ug.cId]))*(iinv.bp[ug.cId]+iinv.mp[ug.cId] - iinv.pp1[ug.cId]* (0.01+iinv.spp[ug.cId]));
	}

	//iinv.res_p = sqrt(iinv.res_p);
	//iinv.res_p = 0;
}


void UINsInvterm::UpdateFaceflux()
{

	for (int fId = ug.nBFace; fId < ug.nFace; ++fId)
	{
		ug.fId = fId;

		ug.lc = (*ug.lcf)[ug.fId];
		ug.rc = (*ug.rcf)[ug.fId];

		CmpUpdateINsFaceflux();
	    
		CmpDun();
	}

	for (int ir = 0; ir < ug.nRegion; ++ir)
	{
		ug.ir = ir;
		ug.bctype = ug.bcRecord->bcInfo->bcType[ir];
		ug.nRBFace = ug.bcRecord->bcInfo->bcFace[ir].size();

		for (int ibc = 0; ibc < ug.nRBFace; ++ibc)
		{
			ug.bcfId = ibc;
			ug.lc = (*ug.lcf)[ug.fId];
			ug.rc = (*ug.rcf)[ug.fId];
			BcInfo* bcInfo = ug.bcRecord->bcInfo;

			ug.fId = bcInfo->bcFace[ug.ir][ibc];
			ug.bcr = bcInfo->bcRegion[ug.ir][ibc];
			ug.bcdtkey = bcInfo->bcdtkey[ug.ir][ibc];

			if (ug.bcr == -1) return; //interface
			int dd = bcdata.r2d[ug.bcr];
			if (dd != -1)
			{
				ug.bcdtkey = 1;
				inscom.bcflow = &bcdata.dataList[dd];
			}

			CmpUpdateINsBcFaceflux();

			CmpDun();
		}
	}

	/*RealField massflux = 0;
	massflux.resize(ug.nCell);
	for (int cId = 0; cId < ug.nCell; cId++)
	{
		int fn = (*ug.c2f)[cId].size();
		for (int iFace = 0; iFace < fn; iFace++)
		{
			int fId = (*ug.c2f)[cId][iFace];
			massflux[cId] += iinv.fq[fId];
		}
		std::cout << "cId: " << cId << ", massflux[cId]: " << massflux[cId] << std::endl;
	}*/

}

void UINsInvterm::CmpUpdateINsBcFaceflux()
{
	if (ug.bctype == BC::SOLID_SURFACE)
	{
		iinv.fq[ug.fId] = 0;
	}

	else if (ug.bctype == BC::INFLOW)
	{
		iinv.fq[ug.fId] += 0;
	}

	else if (ug.bctype == BC::OUTFLOW)
	{
		Real Df1 = iinv.Vdvu[ug.fId] * (*ug.a1)[ug.fId];
        Real Df2 = iinv.Vdvv[ug.fId] * (*ug.a2)[ug.fId];
        Real Df3 = iinv.Vdvw[ug.fId] * (*ug.a3)[ug.fId];

        Real l2rdx = (*ug.xfc)[ug.fId] - (*ug.xcc)[ug.lc];
        Real l2rdy = (*ug.yfc)[ug.fId] - (*ug.ycc)[ug.lc];
        Real l2rdz = (*ug.zfc)[ug.fId] - (*ug.zcc)[ug.lc];

        Real Df = Df1 * (*ug.a1)[ug.fId] + Df2 * (*ug.a2)[ug.fId] + Df3 * (*ug.a3)[ug.fId];

        Real dist = l2rdx * (*ug.a1)[ug.fId] + l2rdy * (*ug.a2)[ug.fId] + l2rdz * (*ug.a3)[ug.fId];

        iinv.rf = (*uinsf.q)[IIDX::IIR][ug.lc];
		iinv.fux = iinv.rf * Df / dist * (iinv.pp[ug.lc] - iinv.ppf[ug.fId]);
		iinv.fq[ug.fId] = iinv.fq[ug.fId] - iinv.fux;
	}
	//Real Df1 = iinv.Vdvu[ug.fId] * (*ug.a1)[ug.fId];
	//Real Df2 = iinv.Vdvv[ug.fId] * (*ug.a2)[ug.fId];
	//Real Df3 = iinv.Vdvw[ug.fId] * (*ug.a3)[ug.fId];

	//Real l2rdx = (*ug.xfc)[ug.fId] - (*ug.xcc)[ug.lc];
	//Real l2rdy = (*ug.yfc)[ug.fId] - (*ug.ycc)[ug.lc];
	//Real l2rdz = (*ug.zfc)[ug.fId] - (*ug.zcc)[ug.lc];

	//Real Df = Df1 * (*ug.a1)[ug.fId] + Df2 * (*ug.a2)[ug.fId] + Df3 * (*ug.a3)[ug.fId];

	//Real dist = l2rdx * (*ug.a1)[ug.fId] + l2rdy * (*ug.a2)[ug.fId] + l2rdz * (*ug.a3)[ug.fId];

	//iinv.rf = (*uinsf.q)[IIDX::IIR][ug.lc];
	//iinv.fux = iinv.rf * Df / dist * (iinv.pp[ug.lc] - iinv.ppf[ug.fId]);
	//iinv.fq[ug.fId] = iinv.fq[ug.fId] - iinv.fux;

}


void UINsInvterm::CmpUpdateINsFaceflux()
{
	/*Real Df1 = iinv.Vdvu[ug.fId] * (*ug.a1)[ug.fId];
	Real Df2 = iinv.Vdvv[ug.fId] * (*ug.a2)[ug.fId];
	Real Df3 = iinv.Vdvw[ug.fId] * (*ug.a3)[ug.fId];

	Real l2rdx = (*ug.xcc)[ug.rc] - (*ug.xcc)[ug.lc];
	Real l2rdy = (*ug.ycc)[ug.rc] - (*ug.ycc)[ug.lc];
	Real l2rdz = (*ug.zcc)[ug.rc] - (*ug.zcc)[ug.lc];

	Real Df = Df1 * (*ug.a1)[ug.fId] + Df2 * (*ug.a2)[ug.fId] + Df3 * (*ug.a3)[ug.fId];

	Real dist = l2rdx * (*ug.a1)[ug.fId] + l2rdy * (*ug.a2)[ug.fId] + l2rdz * (*ug.a3)[ug.fId];

	iinv.rf = (*uinsf.q)[IIDX::IIR][ug.lc];*/

	iinv.fux = iinv.duf[ug.fId] * (iinv.pp[ug.lc] - iinv.pp[ug.rc]);
	iinv.fq[ug.fId] = iinv.fq[ug.fId] + iinv.fux;
}

void UINsInvterm::CmpDun()
{
	if (ug.fId > ug.nBFace - 1)
	{
		iinv.uf[ug.fId] = (*ug.fl)[ug.fId] * (*uinsf.q)[IIDX::IIU][ug.lc] + (*ug.fr)[ug.fId] * (*uinsf.q)[IIDX::IIU][ug.rc];
		iinv.vf[ug.fId] = (*ug.fl)[ug.fId] * (*uinsf.q)[IIDX::IIV][ug.lc] + (*ug.fr)[ug.fId] * (*uinsf.q)[IIDX::IIV][ug.rc];
		iinv.wf[ug.fId] = (*ug.fl)[ug.fId] * (*uinsf.q)[IIDX::IIW][ug.lc] + (*ug.fr)[ug.fId] * (*uinsf.q)[IIDX::IIW][ug.rc];
		Real un = iinv.uf[ug.fId] * (*ug.a1)[ug.fId] + iinv.vf[ug.fId] * (*ug.a2)[ug.fId] + iinv.wf[ug.fId] * (*ug.a3)[ug.fId];
		
		/*iinv.rf = (*uinsf.q)[IIDX::IIR][ug.lc];
		iinv.dun[ug.fId] = iinv.fq[ug.fId] / (iinv.rf + SMALL) - un;*/

		iinv.dun[ug.fId] = iinv.fq[ug.fId]  - un;
	}

	else 
	{
		if (ug.bctype == BC::INFLOW)
		{
			iinv.uf[ug.fId] = inscom.inflow[1];

			iinv.vf[ug.fId] = inscom.inflow[2];

			iinv.wf[ug.fId] = inscom.inflow[3];

			iinv.pf[ug.fId] = inscom.inflow[4];
		}

		else if (ug.bctype == BC::SOLID_SURFACE)
		{
			if (ug.bcdtkey == 0)     //静止流动状态时固壁边界面的速度应该为零
			{
				iinv.uf[ug.fId] = (*ug.vfx)[ug.fId];

				iinv.vf[ug.fId] = (*ug.vfy)[ug.fId];

				iinv.wf[ug.fId] = (*ug.vfz)[ug.fId];
			}
			else
			{
				iinv.uf[ug.fId] = (*inscom.bcflow)[1];

				iinv.vf[ug.fId] = (*inscom.bcflow)[2];

				iinv.wf[ug.fId] = (*inscom.bcflow)[3];
			}
		}

		else if (ug.bctype == BC::OUTFLOW)
		{
			iinv.uf[ug.cId] = (*uinsf.q)[IIDX::IIU][ug.lc];

			iinv.vf[ug.cId] = (*uinsf.q)[IIDX::IIV][ug.lc];

			iinv.wf[ug.cId] = (*uinsf.q)[IIDX::IIW][ug.lc];
		}

		else if (ug.bctype == BC::FIXP)
		{
			int lc = (*ug.lcf)[ug.fId];

			ONEFLOW::CmpINsGrad(iinv.pf, iinv.dpdx, iinv.dpdy, iinv.dpdz);
			iinv.uf[ug.fId] = iinv.uf[ug.fId] - iinv.dpdx[lc] * iinv.VdU[lc];
			iinv.vf[ug.fId] = iinv.vf[ug.fId] - iinv.dpdy[lc] * iinv.VdV[lc];
			iinv.wf[ug.fId] = iinv.wf[ug.fId] - iinv.dpdz[lc] * iinv.VdW[lc];
		}

		(*uinsf.q)[IIDX::IIU][ug.rc] = iinv.uf[ug.fId];
		(*uinsf.q)[IIDX::IIV][ug.rc] = iinv.vf[ug.fId];
		(*uinsf.q)[IIDX::IIW][ug.rc] = iinv.wf[ug.fId];

		/*Real un = iinv.uf[ug.fId] * (*ug.a1)[ug.fId] + iinv.vf[ug.fId] * (*ug.a2)[ug.fId] + iinv.wf[ug.fId] * (*ug.a3)[ug.fId];
		iinv.rf = (*uinsf.q)[IIDX::IIR][ug.lc];
		iinv.dun[ug.fId] = iinv.fq[ug.fId] / (iinv.rf + SMALL) - un;*/
	}
}

void UINsInvterm::UpdateSpeed()
{
	RealField dqqdx, dqqdy, dqqdz;
	dqqdx.resize(ug.nCell);
	dqqdy.resize(ug.nCell);
	dqqdz.resize(ug.nCell);
	ONEFLOW::CmpINsGrad(iinv.ppf, dqqdx, dqqdy, dqqdz);

	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		iinv.uu[cId] = iinv.VdU[cId] * dqqdx[cId]; 
		iinv.vv[cId] = iinv.VdV[cId] * dqqdy[cId];
		iinv.ww[cId] = iinv.VdW[cId] * dqqdz[cId];

		(*uinsf.q)[IIDX::IIU][cId] -= iinv.uu[cId];
		(*uinsf.q)[IIDX::IIV][cId] -= iinv.vv[cId];
		(*uinsf.q)[IIDX::IIW][cId] -= iinv.ww[cId];

	}

	
}

void UINsInvterm::UpdateINsRes()
{

	std::cout << "iinv.remax_up:" << iinv.remax_up << std::endl;
	std::cout << "iinv.remax_vp:" << iinv.remax_vp << std::endl;
	std::cout << "iinv.remax_wp:" << iinv.remax_wp << std::endl;
	std::cout << "iinv.remax_pp:" << iinv.remax_pp << std::endl;

	ofstream fileres_up("residual_up.txt", ios::app);
	//fileres_p << "residual_p:" <<residual_p << endl;
	fileres_up << iinv.remax_up << endl;
	fileres_up.close();

	ofstream fileres_vp("residual_vp.txt", ios::app);
	//fileres_p << "residual_p:" <<residual_p << endl;
	fileres_vp << iinv.remax_vp << endl;
	fileres_vp.close();

	ofstream fileres_wp("residual_wp.txt", ios::app);
	//fileres_p << "residual_p:" <<residual_p << endl;
	fileres_wp << iinv.remax_wp << endl;
	fileres_wp.close();

	ofstream fileres_pp("residual_pp.txt", ios::app);
	//fileres_p << "residual_p:" <<residual_p << endl;
	fileres_pp << iinv.remax_pp << endl;
	fileres_pp.close();
}


void UINsInvterm::CmpPreGrad()
{
	iinv.dqqdx = 0;
	iinv.dqqdy = 0;
	iinv.dqqdz = 0;

	for (int fId = ug.nBFace; fId < ug.nFace; ++fId)
	{
		ug.fId = fId;
		ug.lc = (*ug.lcf)[ug.fId];
		ug.rc = (*ug.rcf)[ug.fId];

		Real dxl = (*ug.xfc)[ug.fId] - (*ug.xcc)[ug.lc];
		Real dyl = (*ug.yfc)[ug.fId] - (*ug.ycc)[ug.lc];
		Real dzl = (*ug.zfc)[ug.fId] - (*ug.zcc)[ug.lc];

		Real dxr = (*ug.xfc)[ug.fId] - (*ug.xcc)[ug.rc];
		Real dyr = (*ug.yfc)[ug.fId] - (*ug.ycc)[ug.rc];
		Real dzr = (*ug.zfc)[ug.fId] - (*ug.zcc)[ug.rc];

		Real delt1 = DIST(dxl, dyl, dzl);
		Real delt2 = DIST(dxr, dyr, dzr);
		Real delta = 1.0 / (delt1 + delt2 + SMALL);

		Real cl = delt2 * delta;
		Real cr = delt1 * delta;

		iinv.value = cl * iinv.pp[ug.lc] + cr * iinv.pp[ug.rc];

		Real fnxa = (*ug.xfn)[ug.fId] * (*ug.farea)[ug.fId];
		Real fnya = (*ug.yfn)[ug.fId] * (*ug.farea)[ug.fId];
		Real fnza = (*ug.zfn)[ug.fId] * (*ug.farea)[ug.fId];

		iinv.dqqdx[ug.lc] += fnxa * iinv.value;
		iinv.dqqdy[ug.lc] += fnya * iinv.value;
		iinv.dqqdz[ug.lc] += fnza * iinv.value;

		//if (ug.fId < ug.nBFace) continue;

		iinv.dqqdx[ug.rc] += -fnxa * iinv.value;
		iinv.dqqdy[ug.rc] += -fnya * iinv.value;
		iinv.dqqdz[ug.rc] += -fnza * iinv.value;
	}

	for (int fId = 0; fId < ug.nBFace; ++fId)
	{
		ug.fId = fId;
		ug.lc = (*ug.lcf)[ug.fId];
		ug.rc = (*ug.rcf)[ug.fId];

		iinv.value = iinv.ppf[ug.fId];

		Real fnxa = (*ug.xfn)[ug.fId] * (*ug.farea)[ug.fId];
		Real fnya = (*ug.yfn)[ug.fId] * (*ug.farea)[ug.fId];
		Real fnza = (*ug.zfn)[ug.fId] * (*ug.farea)[ug.fId];

		iinv.dqqdx[ug.lc] += fnxa * iinv.value;
		iinv.dqqdy[ug.lc] += fnya * iinv.value;
		iinv.dqqdz[ug.lc] += fnza * iinv.value;
	}

	for (int cId = 0; cId < ug.nCell; ++cId)
	{
		ug.cId = cId;
		Real ovol = one / (*ug.cvol)[ug.cId];
		iinv.dqqdx[ug.cId] *= ovol;
		iinv.dqqdy[ug.cId] *= ovol;
		iinv.dqqdz[ug.cId] *= ovol;
	}

	/*for (int fId = 0; fId < ug.nBFace; ++fId)
	{
		ug.fId = fId;
		ug.lc = (*ug.lcf)[ug.fId];
		ug.rc = (*ug.rcf)[ug.fId];

		//if (ug.rc > ug.nCell)
		//{
		iinv.dqqdx[ug.rc] = iinv.dqqdx[ug.lc];
		iinv.dqqdy[ug.rc] = iinv.dqqdy[ug.lc];
		iinv.dqqdz[ug.rc] = iinv.dqqdz[ug.lc];
		//}

	}*/

}


void UINsInvterm::Alloc()
{
	uinsf.qf = new MRField(inscom.nEqu, ug.nFace);
}

void UINsInvterm::DeAlloc()
{
	delete uinsf.qf;
}


void UINsInvterm::ReadTmp()
{
	static int iii = 0;
	if (iii) return;
	iii = 1;
	fstream file;
	file.open("nsflow.dat", ios_base::in | ios_base::binary);
	if (!file) exit(0);

	uinsf.Init();

	for (int cId = 0; cId < ug.nTCell; ++cId)
	{
		for (int iEqu = 0; iEqu < 5; ++iEqu)
		{
			file.read(reinterpret_cast<char*>(&(*uinsf.q)[iEqu][cId]), sizeof(double));
		}
	}

	for (int cId = 0; cId < ug.nTCell; ++cId)
	{
		file.read(reinterpret_cast<char*>(&(*uinsf.visl)[0][cId]), sizeof(double));
	}

	for (int cId = 0; cId < ug.nTCell; ++cId)
	{
		file.read(reinterpret_cast<char*>(&(*uinsf.vist)[0][cId]), sizeof(double));
	}

	vector< Real > tmp1(ug.nTCell), tmp2(ug.nTCell);

	for (int cId = 0; cId < ug.nTCell; ++cId)
	{
		tmp1[cId] = (*uinsf.timestep)[0][cId];
	}

	for (int cId = 0; cId < ug.nTCell; ++cId)
	{
		file.read(reinterpret_cast<char*>(&(*uinsf.timestep)[0][cId]), sizeof(double));
	}

	for (int cId = 0; cId < ug.nTCell; ++cId)
	{
		tmp2[cId] = (*uinsf.timestep)[0][cId];
	}

	turbcom.Init();
	uturbf.Init();
	for (int iCell = 0; iCell < ug.nTCell; ++iCell)
	{
		for (int iEqu = 0; iEqu < turbcom.nEqu; ++iEqu)
		{
			file.read(reinterpret_cast<char*>(&(*uturbf.q)[iEqu][iCell]), sizeof(double));
		}
	}
	file.close();
	file.clear();
}



EndNameSpace