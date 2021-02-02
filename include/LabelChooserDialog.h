/*
 * LabelChooserDialog.h
 *
 *  Copyright (C) 2010,2011 Stefan Bolus, University of Kiel, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef LABELCHOOSERDIALOG_H_
#define LABELCHOOSERDIALOG_H_

#include "label.h"

gboolean label_chooser_isLabelSelected (GString * sel);

#if 0
gboolean label_chooser_isAutoSelected (GString * sel);
gboolean label_chooser_isNoneSelected (GString * sel);
gboolean label_chooser_isLabelSelected (GString * sel);

gboolean
label_chooser_dialog (Rel * rel,
                      GString * /*out*/ rowLabelName,
                      GString * /*out*/ colLabelName);
#endif

gboolean
label_chooser_dialog_preset (GString * /*inout*/ rowLabelName,
                             GString * /*inout*/ colLabelName);

#endif /* LABELCHOOSERDIALOG_H_ */
