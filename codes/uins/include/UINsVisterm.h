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


#pragma once
#include "INsVisterm.h"
#include "HXArray.h"

BeginNameSpace( ONEFLOW )

class UINsVisterm : public INsVisterm
{
public:
	UINsVisterm();
    ~UINsVisterm();
public:
    typedef void ( UINsVisterm:: * VisPointer )();
    VisPointer visPointer;
    MRField * visflux;

public:
    void CmpViscoff();
    void PrepareField();
    void CmpVisGrad();
	void CmpPreandVisGrad();
    
	void CmpVisterm();
	void CmpFaceVisterm(RealField& dudx, RealField& dudy, RealField& dudz, RealField& dvdx, RealField& dvdy, RealField& dvdz, RealField& dwdx, RealField& dwdy, RealField& dwdz);
	void CmpBcFaceVisterm(RealField& dudx, RealField& dudy, RealField& dudz, RealField& dvdx, RealField& dvdy, RealField& dvdz, RealField& dwdx, RealField& dwdy, RealField& dwdz);

    void Alloc();
    void DeAlloc();
	void CmpINsSrc();
    void DifEquaMom();
    void RelaxMom(Real a);
	//void Addcoff();

	void CmpTranst();
public:
    void PrepareFaceValue();
    void SaveFacePara();
    void CmpFaceWeight();
public:
    void CmpGradCoef();
    void PrepareCellGeom();
};

void ICmpLaminarViscosity( int flag );



EndNameSpace