/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include <esc/common.h>
#include <gui/uielement.h>

namespace gui {
	class Panel;
	class ScrollPane;
	class Layout;

	/**
	 * The abstract base class for all controls
	 */
	class Control : public UIElement {
		friend class Panel;
		friend class ScrollPane;
		friend class Layout;

	public:
		/**
		 * Creates an control at position 0,0 and 0x0 pixels large. When using a layout, this
		 * will determine the actual position and size.
		 */
		Control()
			: UIElement() {
		}
		/**
		 * Constructor that specifies a position and size explicitly. This can be used if no layout
		 * is used or if a different preferred size than the min size is desired.
		 *
		 * @param pos the position
		 * @param size the size
		 */
		Control(const Pos &pos,const Size &size)
			: UIElement(pos,size) {
		}

		/**
		 * Does nothing
		 */
		virtual void layout() {
		}

		/**
		 * Is called as soon as this control received the focus
		 */
		virtual void onFocusGained() {
			getParent()->setFocus(this);
		}
		/**
		 * Is called as soon as this control lost the focus
		 */
		virtual void onFocusLost() {
			getParent()->setFocus(nullptr);
		}

	protected:
		/**
		 * Resizes the ui-element to given size
		 *
		 * @param size the new size
		 */
		virtual void resizeTo(const Size &size);
		/**
		 * Moves the ui-element to x,y.
		 *
		 * @param x the new x-position
		 * @param y the new y-position
		 */
		virtual void moveTo(const Pos &pos);

		/**
		 * @return the control that has the focus (not a panel!) or nullptr if no one
		 */
		virtual Control *getFocus() {
			// panel returns the focused control on the panel; a control does already return itself
			return this;
		}
		virtual const Control *getFocus() const {
			return this;
		}

		/**
		 * Sets the parent of this control (used by Panel)
		 *
		 * @param e the parent
		 */
		virtual void setParent(UIElement *e);

		/**
		 * Updates the paint-region of this control
		 */
		virtual void setRegion();

	private:
		Pos getParentOff(UIElement *c) const;
	};
}
