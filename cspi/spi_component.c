/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 *
 * AccessibleComponent function implementations
 *
 */

#include <cspi/spi-private.h>

/**
 * AccessibleComponent_ref:
 * @obj: a pointer to an object implementing #AccessibleComponent on which to operate.
 *
 * Increment the reference count for an #AccessibleComponent.
 **/
void
AccessibleComponent_ref (AccessibleComponent *obj)
{
  cspi_object_ref (obj);
}

/**
 * AccessibleComponent_unref:
 * @obj: a pointer to the object implementing #AccessibleComponent on which to operate.
 *
 * Decrement the reference count for an #AccessibleComponent.
 **/
void
AccessibleComponent_unref (AccessibleComponent *obj)
{
  cspi_object_unref (obj);
}

/**
 * AccessibleComponent_contains:
 * @obj: a pointer to the #AccessibleComponent to query.
 * @x: a #long specifying the x coordinate in question.
 * @y: a #long specifying the y coordinate in question.
 * @ctype: the desired coordinate system of the point (@x, @y)
 *         (e.g. CSPI_COORD_TYPE_WINDOW, CSPI_COORD_TYPE_SCREEN).
 *
 * Query whether a given #AccessibleComponent contains a particular point.
 *
 * Returns: a #TRUE if the specified component contains the point (@x, @y),
 *          otherwise #FALSE.
 **/
SPIBoolean
AccessibleComponent_contains (AccessibleComponent *obj,
                              long int x,
                              long int y,
                              AccessibleCoordType ctype)
{
  SPIBoolean retval;

  cspi_return_val_if_fail (obj != NULL, FALSE);

  retval = Accessibility_Component_contains (CSPI_OBJREF (obj),
					     x,
					     y,
					     ctype,
					     cspi_ev ());
  cspi_return_val_if_ev ("contains", FALSE);

  return retval;
}

/**
 * AccessibleComponent_getAccessibleAtPoint:
 * @obj: a pointer to the #AccessibleComponent to query.
 * @x: a #long specifying the x coordinate of the point in question.
 * @y: a #long specifying the y coordinate of the point in question.
 * @ctype: the coordinate system of the point (@x, @y)
 *         (e.g. CSPI_COORD_TYPE_WINDOW, CSPI_COORD_TYPE_SCREEN).
 *
 * Get the accessible child at a given coordinate within an #AccessibleComponent.
 *
 * Returns: a pointer to an #Accessible child of the specified component which
 *          contains the point (@x, @y), or NULL of no child contains the point.
 **/
Accessible *
AccessibleComponent_getAccessibleAtPoint (AccessibleComponent *obj,
                                          long int x,
                                          long int y,
                                          AccessibleCoordType ctype)
{
  Accessibility_Accessible child;

  cspi_return_val_if_fail (obj != NULL, NULL);

  child = Accessibility_Component_getAccessibleAtPoint (CSPI_OBJREF (obj),
							x,
							y,
							ctype,
							cspi_ev ());
  return cspi_object_add (child);
}

/**
 * AccessibleComponent_getExtents:
 * @obj: a pointer to the #AccessibleComponent to query.
 * @x: a pointer to a #long into which the minimum x coordinate will be returned.
 * @y: a pointer to a #long into which the minimum y coordinate will be returned.
 * @width: a pointer to a #long into which the x extents (width) will be returned.
 * @height: a pointer to a #long into which the y extents (height) will be returned.
 * @ctype: the desired coordinate system into which to return the results,
 *         (e.g. CSPI_COORD_TYPE_WINDOW, CSPI_COORD_TYPE_SCREEN).
 *
 * Get the bounding box of the specified #AccessibleComponent.
 *
 **/
void
AccessibleComponent_getExtents (AccessibleComponent *obj,
                                long int *x,
                                long int *y,
                                long int *width,
                                long int *height,
                                AccessibleCoordType ctype)
{
  Accessibility_BoundingBox bbox;

  cspi_return_if_fail (obj != NULL);

  bbox = Accessibility_Component_getExtents (CSPI_OBJREF (obj),
					     ctype,
					     cspi_ev ());
  if (!cspi_check_ev ("getExtents"))
    {
      *x = *y = *width = *height = 0;    
    }
  else
    {
      *x = bbox.x;
      *y = bbox.y;
      *width = bbox.width;
      *height = bbox.height;
    }
}

/**
 * AccessibleComponent_getPosition:
 * @obj: a pointer to the #AccessibleComponent to query.
 * @x: a pointer to a #long into which the minimum x coordinate will be returned.
 * @y: a pointer to a #long into which the minimum y coordinate will be returned.
 * @ctype: the desired coordinate system into which to return the results,
 *         (e.g. CSPI_COORD_TYPE_WINDOW, CSPI_COORD_TYPE_SCREEN).
 *
 * Get the minimum x and y coordinates of the specified #AccessibleComponent.
 *
 **/
void
AccessibleComponent_getPosition (AccessibleComponent *obj,
                                 long int *x,
                                 long int *y,
                                 AccessibleCoordType ctype)
{
  CORBA_long cx, cy;

  cspi_return_if_fail (obj != NULL);

  Accessibility_Component_getPosition (CSPI_OBJREF (obj),
				       &cx, &cy, ctype, cspi_ev ());

  if (!cspi_check_ev ("getPosition"))
    {
      *x = *y = 0;
    }
  else
    {
      *x = cx;
      *y = cy;
    }
}

/**
 * AccessibleComponent_getSize:
 * @obj: a pointer to the #AccessibleComponent to query.
 * @width: a pointer to a #long into which the x extents (width) will be returned.
 * @height: a pointer to a #long into which the y extents (height) will be returned.
 *
 * Get the size of the specified #AccessibleComponent.
 *
 **/
void
AccessibleComponent_getSize (AccessibleComponent *obj,
                             long int *width,
                             long int *height)
{
  CORBA_long cw, ch;

  cspi_return_if_fail (obj != NULL);

  Accessibility_Component_getSize (CSPI_OBJREF (obj),
                                   &cw,
                                   &ch,
                                   cspi_ev ());
  if (cspi_check_ev ("getSize"))
  {
    *width = *height = 0;
  }
  else
  {
    *width = cw;
    *height = ch;
  }
}

/**
 * AccessibleComponent_getLayer:
 * @obj: a pointer to the #AccessibleComponent to query.
 *
 * Query which layer the component is painted into, to help determine its 
 *      visibility in terms of stacking order.
 *
 * Returns: the #AccessibleComponentLayer into which this component is painted.
 **/
AccessibleComponentLayer
AccessibleComponent_getLayer (AccessibleComponent *obj)
{
  AccessibleComponentLayer     retval;
  Accessibility_ComponentLayer zlayer;

  cspi_return_val_if_fail (obj != NULL, FALSE);

  zlayer = Accessibility_Component_getLayer (CSPI_OBJREF (obj),
					     cspi_ev ());

  cspi_return_val_if_ev ("getLayer", SPI_LAYER_INVALID);

  switch (zlayer)
    {
    case Accessibility_LAYER_BACKGROUND:
      retval = SPI_LAYER_BACKGROUND;
      break;
    case Accessibility_LAYER_CANVAS:	  
      retval = SPI_LAYER_CANVAS;
      break;
    case Accessibility_LAYER_WIDGET:	  
      retval = SPI_LAYER_WIDGET;
      break;
    case Accessibility_LAYER_MDI:	  
      retval = SPI_LAYER_MDI;
      break;
    case Accessibility_LAYER_POPUP:	  
      retval = SPI_LAYER_POPUP;
      break;
    case Accessibility_LAYER_OVERLAY:	  
      retval = SPI_LAYER_OVERLAY;
      break;
    default:
      retval = SPI_LAYER_INVALID;
      break;
    }

  return retval;
}

/**
 * AccessibleComponent_getMDIZOrder:
 * @obj: a pointer to the #AccessibleComponent to query.
 *
 * Query the z stacking order of a component which is in the MDI layer.
 *       (Bigger z-order numbers mean nearer the top)
 *
 * Returns: a short integer indicating the stacking order of the component 
 *       in the MDI layer, or -1 if the component is not in the MDI layer.
 **/
short
AccessibleComponent_getMDIZOrder (AccessibleComponent *obj)
{
  short retval;

  cspi_return_val_if_fail (obj != NULL, FALSE);

  retval = Accessibility_Component_getMDIZOrder (CSPI_OBJREF (obj),
						 cspi_ev ());

  cspi_return_val_if_ev ("getMDIZOrder", FALSE);

  return retval;
}

/**
 * AccessibleComponent_grabFocus:
 * @obj: a pointer to the #AccessibleComponent on which to operate.
 *
 * Attempt to set the keyboard input focus to the specified
 *         #AccessibleComponent.
 *
 * Returns: #TRUE if successful, #FALSE otherwise.
 *
 **/
SPIBoolean
AccessibleComponent_grabFocus (AccessibleComponent *obj)
{
  SPIBoolean retval;

  cspi_return_val_if_fail (obj != NULL, FALSE);

  retval = Accessibility_Component_grabFocus (CSPI_OBJREF (obj),
					      cspi_ev ());

  cspi_return_val_if_ev ("grabFocus", FALSE);

  return retval;
}

