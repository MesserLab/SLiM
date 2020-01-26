//
//  QtSLiMAbout.h
//  SLiM
//
//  Created by Ben Haller on 8/2/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.

#ifndef QTSLIMABOUT_H
#define QTSLIMABOUT_H

#include <QDialog>

namespace Ui {
class QtSLiMAbout;
}

class QtSLiMAbout : public QDialog
{
    Q_OBJECT
    
public:
    explicit QtSLiMAbout(QWidget *parent = nullptr);
    ~QtSLiMAbout();
    
private:
    Ui::QtSLiMAbout *ui;
};

#endif // QTSLIMABOUT_H
