/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2001, 2002 Sun Microsystems Inc.,
 * Copyright 2001, 2002 Ximian, Inc.
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

#ifndef _SPI_H
#define _SPI_H

#include <cspi/spi-impl.h>
/*
 * Definitions for AccessibleRole, AccessibleState,
 * and event listeners.
 */
#include <cspi/spi-roletypes.h>
#include <cspi/spi-statetypes.h>
#include <cspi/spi-listener.h>

/*
 * Auxiliary typedefs and mask definitions
 */
#include <libspi/keymasks.h>

#ifdef  __cplusplus
extern "C" {
#endif

/**
 *AccessibleCollectionMatchType:
 *
 *@SPI_COLLECTION_MATCH_INVALID 	
 *@SPI_COLLECTION_MATCH_ALL 	TRUE if all of the criteria are met
 *@SPI_COLLECTION_MATCH_ANY 	TRUE if any of the criteria are met
 *@SPI_COLLECTION_MATCH_NONE 	TRUE if none of the criteria are met
 *@SPI_COLLECTION_MATCH_EMPTY 	Same as MATCH_ALL if the criteria is non-empty; 
 * for empty criteria this rule requires returned value to also have empty set.
 *@SPI_COLLECTION_MATCH_LAST_DEFINED 	
 *
 **/
typedef enum
{
     SPI_COLLECTION_MATCH_INVALID,
     SPI_COLLECTION_MATCH_ALL,
     SPI_COLLECTION_MATCH_ANY,
     SPI_COLLECTION_MATCH_NONE,
     SPI_COLLECTION_MATCH_EMPTY,
     SPI_COLLECTION_MATCH_LAST_DEFINED
}AccessibleCollectionMatchType;


/**
 *AccessibleTextBoundaryType:
 *@SPI_TEXT_BOUNDARY_CHAR: Delimiter is the current character's bounds.
 *@SPI_TEXT_BOUNDARY_CURSOR_POS: Delimiter is the current text caret position.
 *@SPI_TEXT_BOUNDARY_WORD_START: Bounds run from the first character of a word to the first 
 * character of the following word (i.e. including trailing whitespace, if any) within the text object.
 *@SPI_TEXT_BOUNDARY_WORD_END: Bounds run from the last character of a word to the last 
 * character of the following word (i.e. including leading whitespace) within the text object.
 *@SPI_TEXT_BOUNDARY_SENTENCE_START: Bounds run from the first character of a sentence to the first 
 * character of the next sentence in the text object.
 *@SPI_TEXT_BOUNDARY_SENTENCE_END: Bounds run from the last character of a sentence to the last 
 * character of the next sentence in the text object.
 *@SPI_TEXT_BOUNDARY_LINE_START: Bounds span one line of text, with the line delimiter at the end of the
 * bounds, if present, within the current text object.
 *@SPI_TEXT_BOUNDARY_LINE_END: Bounds span one line of text, including a preceding line delimiter
 * if present within the current text object.
 *@SPI_TEXT_BOUNDARY_ATTRIBUTE_RANGE: Bounds span the run, relative to the specified offset and
 * text accessor API rules, over which the attributes of the text object are invariant.  
 *
 *Text boundary types used for specifying boundaries for regions of text.
 **/
typedef enum
{
  SPI_TEXT_BOUNDARY_CHAR,
  SPI_TEXT_BOUNDARY_CURSOR_POS,
  SPI_TEXT_BOUNDARY_WORD_START,
  SPI_TEXT_BOUNDARY_WORD_END,
  SPI_TEXT_BOUNDARY_SENTENCE_START,
  SPI_TEXT_BOUNDARY_SENTENCE_END,
  SPI_TEXT_BOUNDARY_LINE_START,
  SPI_TEXT_BOUNDARY_LINE_END,
  SPI_TEXT_BOUNDARY_ATTRIBUTE_RANGE
} AccessibleTextBoundaryType;

/**
 *AccessibleTextClipType
 *@SPI_TEXT_CLIP_NONE: No clipping to be done
 *@SPI_TEXT_CLIP_MIN: Text clipped by min coordinate is omitted
 *@SPI_TEXT_CLIP_MAX: Text clipped by max coordinate is omitted
 *@SPI_TEXT_CLIP_BOTH: Only text fully within mix/max bound is retained
 *
 *Describes the type of clipping required.
 **/
typedef enum
{
  SPI_TEXT_CLIP_NONE,
  SPI_TEXT_CLIP_MIN,
  SPI_TEXT_CLIP_MAX,
  SPI_TEXT_CLIP_BOTH
} AccessibleTextClipType;

/**
 *AccessibleRelationType:
 *@SPI_RELATION_NULL: Not used, this is an invalid value for this enumeration.
 *@SPI_RELATION_LABEL_FOR: Indicates an object is a label for one or more target objects.
 *@SPI_RELATION_LABELED_BY: Indicates an object is labelled by one or more target objects.
 *@SPI_RELATION_CONTROLLED_BY: Indicates an object controlled by one or more target objects.
 *@SPI_RELATION_CONTROLLER_FOR: Indicates an object is an controller for one or more target objects.
 *@SPI_RELATION_MEMBER_OF: Indicates an object is a member of a group of one or
more target objects.
 *@SPI_RELATION_NODE_CHILD_OF: Indicates an object is a cell in a treetable which is displayed because a cell in the same column is expanded and identifies that cell.
 *@SPI_RELATION_EXTENDED: This value indicates that a relation other than those pre-specified by this version of AT-SPI
 * is present.
 *@SPI_RELATION_FLOWS_TO: Indicates that the object has content that flows logically to another
 *  AtkObject in a sequential way, (for instance text-flow).
 *@SPI_RELATION_FLOWS_FROM: Indicates that the object has content that flows logically from
 *  another AtkObject in a sequential way, (for instance text-flow).
 *@SPI_RELATION_SUBWINDOW_OF: Indicates a subwindow attached to a component but otherwise has no connection in the UI heirarchy to that component.
 *@SPI_RELATION_EMBEDS: Indicates that the object visually embeds
 *  another object's content, i.e. this object's content flows around
 *  another's content.
 *@SPI_RELATION_EMBEDDED_BY: Inverse of %SPI_RELATION_EMBEDS, indicates that
 *  this object's content is visualy embedded in another object.
 *@SPI_RELATION_POPUP_FOR: Indicates that an object is a popup for another object.
 *@SPI_RELATION_PARENT_WINDOW_OF: Indicates that an object is a parent window of another object.
 *@SPI_RELATION_DESCRIBED_BY: Indicates that another object provides descriptive information about this object; more verbose than %SPI_RELATION_LABELLED_BY.
 *@SPI_RELATION_DESCRIPTION_FOR: Indicates that an object provides descriptive information about another object; more verbose than %SPI_RELATION_LABEL_FOR.
 *@SPI_RELATION_LAST_DEFINED: Do not use, this is an implementation detail used
 *  to identify the size of this enumeration.
 *
 *Describes the type of the relation
 **/
typedef enum
{
  SPI_RELATION_NULL,
  SPI_RELATION_LABEL_FOR,
  SPI_RELATION_LABELED_BY,
  SPI_RELATION_CONTROLLER_FOR,
  SPI_RELATION_CONTROLLED_BY,
  SPI_RELATION_MEMBER_OF,
  SPI_RELATION_NODE_CHILD_OF,
  SPI_RELATION_EXTENDED,
  SPI_RELATION_FLOWS_TO,
  SPI_RELATION_FLOWS_FROM,
  SPI_RELATION_SUBWINDOW_OF,
  SPI_RELATION_EMBEDS,
  SPI_RELATION_EMBEDDED_BY,
  SPI_RELATION_POPUP_FOR,
  SPI_RELATION_PARENT_WINDOW_OF,
  SPI_RELATION_DESCRIPTION_FOR,
  SPI_RELATION_DESCRIBED_BY,
  SPI_RELATION_LAST_DEFINED
} AccessibleRelationType;


/* don't change the order of these ! */
/**
 *AccessibleCoordType:
 *@SPI_COORD_TYPE_SCREEN: specifies xy coordinates relative to the screen
 *@SPI_COORD_TYPE_WINDOW: specifies xy coordinates relative to an object's
 * top-level window
 *
 *Specifies how xy coordinates are to be interpreted. Used by functions such
 *as AccessibleComponent_getPosition() and AccessibleText_getCharacterExtents()
 **/
typedef enum {
  SPI_COORD_TYPE_SCREEN,
  SPI_COORD_TYPE_WINDOW
} AccessibleCoordType;

/**
 *AccessibleKeySynthType:
 *@SPI_KEY_PRESS: Generates a keypress event (requires a subsequent #SPI_KEY_RELEASE event)
 *@SPI_KEY_RELEASE: Generates a key-release event
 *@SPI_KEY_PRESSRELEASE: Generates a key press/release event pair.
 *@SPI_KEY_SYM: Injects a "keysym" event into the stream, as if a press/release pair occurred; allows 
 * the user to specify the key via its symbolic name, as opposed to simulating a hardware press of a 
 * specific key.
 *@SPI_KEY_STRING: Injects one or more keysym events into the keyboard buffer, or directly inserts 
 * a string value into the currently focussed text widget, if the widgets supports this.
 * #SPI_KEY_STRING synthesis provides a shortcut for text substring insertion, and also allows the
 * insertion of text which is not currently available via the current keyboard's keymap.
 *
 * Specifies the type of a generated event.
 **/
typedef enum {
  SPI_KEY_PRESS,
  SPI_KEY_RELEASE, 
  SPI_KEY_PRESSRELEASE,
  SPI_KEY_SYM,
  SPI_KEY_STRING
} AccessibleKeySynthType;

/**
 *AccessibleKeyListenerSyncType:
 *@SPI_KEYLISTENER_NOSYNC: Events may be delivered asynchronously, 
 * which means in some cases they may already have been delivered to the 
 * application before the AT client receives the notification.  
 *@SPI_KEYLISTENER_SYNCHRONOUS: Events are delivered synchronously, before the 
 * currently focussed application sees them.  
 *@SPI_KEYLISTENER_CANCONSUME: Events may be consumed by the AT client.  Presumes and
 * requires #SPI_KEYLISTENER_SYNCHRONOUS, incompatible with #SPI_KEYLISTENER_NOSYNC.
 *@SPI_KEYLISTENER_ALL_WINDOWS: Events are received not from the application toolkit layer, but
 * from the device driver or windowing system subsystem; such notifications are 'global' in the 
 * sense that they are not broken or defeated by applications that participate poorly
 * in the accessibility APIs, or not at all; however because of the intrusive nature of
 * such snooping, it can have side-effects on certain older platforms.  If unconditional
 * event notifications, even when inaccessible or "broken" applications have focus, are not
 * required, it may be best to avoid this enum value/flag.
 *
 *Specified the tyupe of a key listener event.
 * Certain of the values above can and should be bitwise-'OR'ed
 * together, observing the compatibility limitations specified in the description of
 * each value.  For instance, #SPI_KEYLISTENER_ALL_WINDOWS | #SPI_KEYLISTENER_CANCONSUME is
 * a commonly used combination which gives the AT complete control over the delivery of matching
 * events.  However, such filters should be used sparingly as they may have a negative impact on 
 * system performance.
 **/
typedef enum {
  SPI_KEYLISTENER_NOSYNC = 0,
  SPI_KEYLISTENER_SYNCHRONOUS = 1,
  SPI_KEYLISTENER_CANCONSUME = 2,
  SPI_KEYLISTENER_ALL_WINDOWS = 4
} AccessibleKeyListenerSyncType;


/**
 *AccessibleStreamableContentSeekType
 *@SPI_STREAM_SEEK_SET: seek from the 'top' of the streamable
 *@SPI_STREAM_SEEK_CUR: seek from the current position in the stream
 *@SPI_STREAM_SEEK_END: seek from the end of the stream (if known)
 *
 *The seek type for a specified offset in AccessibleStreamableContent_seek.
 **/
typedef enum
{
  SPI_STREAM_SEEK_SET,
  SPI_STREAM_SEEK_CUR,
  SPI_STREAM_SEEK_END
} AccessibleStreamableContentSeekType;

/**
 * SPIException:
 * @type: private
 * @source: private
 * @ev: private
 * @code: private
 * @desc: private
 *
 * An opaque object encapsulating information about thrown exceptions.
 **/
typedef struct _SPIException SPIException;

/**
 * SPIExceptionHandler:
 * @err: points to the SPIException opaque object.
 * @is_fatal: indicates whether the exception is a fatal error or not.
 *
 * A function type for functions to be called when exceptions occur.
 *
 * Returns 
 **/
typedef SPIBoolean (*SPIExceptionHandler) (SPIException *err, SPIBoolean is_fatal);

/**
 * SPIExceptionCode:
 * @SPI_EXCEPTION_UNSPECIFIED: An exception of unknown type, or which doesn't fit the other types.
 * @SPI_EXCEPTION_DISCONNECT: Communication with the object or service has been disconnected;
 * this usually means that the object or service has died or exited.
 * @SPI_EXCEPTION_NO_IMPL: The object or service is missing the implementation for a request.
 * @SPI_EXCEPTION_IO: The communications channel has become corrupted, blocked, or is otherwise in a bad state.
 * @SPI_EXCEPTION_BAD_DATA: The data received or sent over the interface has been identified as 
 * improperly formatted or otherwise fails to match the expectations.
 *
 * Exception codes indicating what's gone wrong in an AT-SPI call.
 **/
typedef enum {
	SPI_EXCEPTION_UNSPECIFIED,
	SPI_EXCEPTION_DISCONNECT,
	SPI_EXCEPTION_NO_IMPL,
	SPI_EXCEPTION_IO,
	SPI_EXCEPTION_BAD_DATA
} SPIExceptionCode;

/**
 * SPIExceptionType:
 * @SPI_EXCEPTION_SOURCE_UNSPECIFIED: Don't know or can't tell where the problem is
 * @SPI_EXCEPTION_SOURCE_ACCESSIBLE: The source of an event or query (i.e. an app) has thrown the exception.
 * @SPI_EXCEPTION_SOURCE_REGISTRY: The AT-SPI registry has thrown the exception or cannot be reached.
 * @SPI_EXCEPTION_SOURCE_DEVICE: The device event subsystem has encountered an error condition.
 *
 * The general source of the failure, i.e. whether the app, registry, or device system has encountered trouble.
 **/
typedef enum {
	SPI_EXCEPTION_SOURCE_UNSPECIFIED,
	SPI_EXCEPTION_SOURCE_ACCESSIBLE,
	SPI_EXCEPTION_SOURCE_REGISTRY,
	SPI_EXCEPTION_SOURCE_DEVICE
} SPIExceptionType;

typedef unsigned long AccessibleKeyEventMask;
typedef unsigned long AccessibleDeviceEventMask;
/**
 * SPIRect:
 * @x: The position of the minimum x value of the rectangle (i.e. left hand corner)
 * @y: The position of the minimum y value of the rectangle's bounds.
 * @width: Width of the rectangle in pixels.
 * @height: Height of the rectangle in pixels.
 *
 * A structure encapsulating a rectangle. 
 **/
typedef struct {
	long x;
	long y;
	long width;
	long height;
} SPIRect;

/**
 *AccessibleComponentLayer:
 *@SPI_LAYER_INVALID: The layer cannot be determined or is somehow undefined.
 *@SPI_LAYER_BACKGROUND: Component belongs to the destop background.
 *@SPI_LAYER_CANVAS: Component is a canvas backdrop or drawing area.
 *@SPI_LAYER_WIDGET: Component is a 'normal' widget.
 *@SPI_LAYER_MDI: Component is drawn in the MDI layer and may have valid
 *                         Z-information relative to other MDI-layer components.
 *@SPI_LAYER_POPUP: Component is in the popup layer, above other widgets and
 *                         MDI components.
 *@SPI_LAYER_OVERLAY: Component is in the overlay plane - this value is reserved
 *                         for future use.
 *@SPI_LAYER_WINDOW: Component is in the window layer and have valid Z-information
 *                   relative to other window-layer components.
 *@SPI_LAYER_LAST_DEFINED: Used to determine the last valid value in the enum,
 *                         should not be encountered.	
 *
 * Describes the layer of a component.
 *
 * These enumerated "layer values" are used when determining which UI
 * rendering layer a component is drawn into, which can help in making
 * determinations of when components occlude one another.
 **/
typedef enum {
    SPI_LAYER_INVALID,
    SPI_LAYER_BACKGROUND,
    SPI_LAYER_CANVAS,
    SPI_LAYER_WIDGET,
    SPI_LAYER_MDI,
    SPI_LAYER_POPUP,
    SPI_LAYER_OVERLAY,
    SPI_LAYER_WINDOW,
    SPI_LAYER_LAST_DEFINED	
} AccessibleComponentLayer;


/**
 * AccessibleTextRange:
 * @start: the first nominal character position within the range.
 * @end: the first nominal character position following the range.
 * @content: The actual text content between @start and @end, as a UTF-8 string.
 *
 * Structure which encapsulates a text range - must be associated with an
 *          AccessibleText-implementing object.
 **/
typedef struct _AccessibleTextRange
{
  long int        start;
  long int        end;
  char           *contents;
} AccessibleTextRange;

/**
 * AccessibleKeySet:
 * @keysyms:
 * @keycodes:
 * @len:
 *
 * Structure containing identifying information about a set of keycode or
 *        keysyms.
 **/
typedef struct _AccessibleKeySet
{
  unsigned long  *keysyms;
  unsigned short *keycodes;
  char          **keystrings;
  short           len;
} AccessibleKeySet;

/*
 * A special value for an AccessibleKeySet type, which tacitly
 * includes all keycodes and keyvals for the specified modifier set.
 */
#define SPI_KEYSET_ALL_KEYS NULL

typedef unsigned long AccessibleModifierMaskType;
/**
 *AccessibleKeyMaskType:
 *
 *AccessibleKeyMaskType is a mask which is a set of key event modifiers
 *which is specified in SPI_registerAccessibleKeystrokeListener.
 **/

typedef AccessibleModifierMaskType AccessibleKeyMaskType;

typedef struct _AccessibleAttributeSet
{
    int len;
    char **attributes;
} AccessibleAttributeSet;

typedef struct _AccessibleRoleSet
{
     int len;
     AccessibleRole *roles;
} AccessibleRoleSet;



/* Basic SPI initialization and event loop function prototypes */

int              SPI_init         (void);
void             SPI_event_main   (void);
void             SPI_event_quit   (void);
SPIBoolean       SPI_eventIsReady (void);
AccessibleEvent *SPI_nextEvent    (SPIBoolean waitForEvent);
int              SPI_exit         (void);

/* Event Listener creation and support.  */

void                      SPI_freeAccessibleKeySet (
	                                           AccessibleKeySet         *keyset);
AccessibleKeySet        * SPI_createAccessibleKeySet (
                                         	   int                       len,
						   const char               *keysyms,
						   short                    *keycodes,
						   const char              **keystrings);
AccessibleEventListener * SPI_createAccessibleEventListener (
	                                           AccessibleEventListenerCB callback,
						   void                     *user_data);
SPIBoolean                AccessibleEventListener_addCallback (
	                                           AccessibleEventListener  *listener,
						   AccessibleEventListenerCB callback,
						   void                     *user_data);
SPIBoolean                AccessibleEventListener_removeCallback (
	                                           AccessibleEventListener  *listener,
						   AccessibleEventListenerCB callback);
void                      AccessibleEventListener_unref (
	                                           AccessibleEventListener  *listener);

/* Device Event Listener creation and support.  */

/* First four are deprecated in favor of the last four; really just a re-name */

AccessibleKeystrokeListener * SPI_createAccessibleKeystrokeListener (
	                                AccessibleKeystrokeListenerCB callback,
					void                         *user_data);
SPIBoolean                    AccessibleKeystrokeListener_addCallback (
	                                AccessibleKeystrokeListener  *listener,
					AccessibleKeystrokeListenerCB callback,
					void                         *user_data);
SPIBoolean                    AccessibleKeystrokeListener_removeCallback (
	                                AccessibleKeystrokeListener *listener,
					AccessibleKeystrokeListenerCB callback);
void                          AccessibleKeystrokeListener_unref (
	                                AccessibleKeystrokeListener *listener);

AccessibleDeviceListener   * SPI_createAccessibleDeviceListener (
	                                AccessibleDeviceListenerCB callback,
					void                      *user_data);
SPIBoolean                    AccessibleDeviceListener_addCallback (
	                                AccessibleDeviceListener  *listener,
					AccessibleDeviceListenerCB callback,
					void                      *user_data);
SPIBoolean                    AccessibleDeviceListener_removeCallback (
	                                AccessibleDeviceListener  *listener,
					AccessibleDeviceListenerCB callback);
void                          AccessibleDeviceListener_unref (
	                                AccessibleDeviceListener *listener);

/* Global functions serviced by the registry */

SPIBoolean SPI_registerGlobalEventListener           (
	                               AccessibleEventListener *listener,
				       const char              *eventType);
SPIBoolean SPI_deregisterGlobalEventListener         (
	                               AccessibleEventListener *listener,
				       const char              *eventType);
SPIBoolean SPI_deregisterGlobalEventListenerAll      (
	                               AccessibleEventListener *listener);
SPIBoolean SPI_registerAccessibleKeystrokeListener   (
                                       AccessibleKeystrokeListener *listener,
				       AccessibleKeySet             *keys,
				       AccessibleKeyMaskType         modmask,
				       AccessibleKeyEventMask        eventmask,
				       AccessibleKeyListenerSyncType sync_type);
SPIBoolean SPI_deregisterAccessibleKeystrokeListener (
	                               AccessibleKeystrokeListener *listener,
				       AccessibleKeyMaskType        modmask);

SPIBoolean SPI_registerDeviceEventListener   (
                                       AccessibleDeviceListener   *listener,
				       AccessibleDeviceEventMask   eventmask,
				       void                       *filter);
SPIBoolean SPI_deregisterDeviceEventListener (
				       AccessibleDeviceListener   *listener,
				       void                       *filter);

int         SPI_getDesktopCount                  (void);
Accessible *SPI_getDesktop                       (int i);
int         SPI_getDesktopList                   (Accessible ***desktop_list);
void        SPI_freeDesktopList                  (Accessible  **desktop_list);

SPIBoolean  SPI_generateKeyboardEvent            (long int                    keyval,
						  char                       *keystring,
						  AccessibleKeySynthType      synth_type);

SPIBoolean  SPI_generateMouseEvent               (long int x, long int y, char *name);

/* Accessible function prototypes  */

void                 Accessible_ref              (Accessible *obj);
void                 Accessible_unref            (Accessible *obj);
char *               Accessible_getName          (Accessible *obj);
char *               Accessible_getDescription   (Accessible *obj);
Accessible *         Accessible_getParent        (Accessible *obj);
long                 Accessible_getChildCount    (Accessible *obj);
Accessible *         Accessible_getChildAtIndex  (Accessible *obj,
						  long int    childIndex);
long                 Accessible_getIndexInParent (Accessible *obj);
AccessibleRelation **Accessible_getRelationSet   (Accessible *obj);
AccessibleRole       Accessible_getRole          (Accessible *obj);
char *               Accessible_getRoleName      (Accessible *obj);
char *               Accessible_getLocalizedRoleName (Accessible *obj);
AccessibleStateSet * Accessible_getStateSet      (Accessible *obj);
AccessibleAttributeSet *Accessible_getAttributes (Accessible *obj);
AccessibleApplication *Accessible_getHostApplication (Accessible *obj);

/* Interface query methods */

SPIBoolean Accessible_isAction            (Accessible *obj);
SPIBoolean Accessible_isApplication       (Accessible *obj);
SPIBoolean Accessible_isCollection        (Accessible *obj);
SPIBoolean Accessible_isComponent         (Accessible *obj);
SPIBoolean Accessible_isDocument          (Accessible *obj);
SPIBoolean Accessible_isEditableText      (Accessible *obj);
SPIBoolean Accessible_isHypertext         (Accessible *obj);
SPIBoolean Accessible_isImage             (Accessible *obj);
SPIBoolean Accessible_isMatchRule         (Accessible *obj);
SPIBoolean Accessible_isSelection         (Accessible *obj);
SPIBoolean Accessible_isStreamableContent (Accessible *obj);
SPIBoolean Accessible_isTable             (Accessible *obj);
SPIBoolean Accessible_isText              (Accessible *obj);
SPIBoolean Accessible_isValue             (Accessible *obj);

AccessibleAction *            Accessible_getAction            (Accessible *obj);
AccessibleApplication *       Accessible_getApplication       (Accessible *obj);
AccessibleCollection *        Accessible_getCollection        (Accessible *obj);
AccessibleComponent *         Accessible_getComponent         (Accessible *obj);
AccessibleDocument *          Accessible_getDocument          (Accessible *obj);
AccessibleEditableText *      Accessible_getEditableText      (Accessible *obj);
AccessibleHypertext *         Accessible_getHypertext         (Accessible *obj);
AccessibleImage *             Accessible_getImage             (Accessible *obj);
AccessibleMatchRule *         Accessible_getMatchRule         (Accessible *obj);
AccessibleSelection *         Accessible_getSelection         (Accessible *obj);
AccessibleStreamableContent * Accessible_getStreamableContent (Accessible *obj);
AccessibleTable *             Accessible_getTable             (Accessible *obj);
AccessibleText *              Accessible_getText              (Accessible *obj);
AccessibleValue *             Accessible_getValue             (Accessible *obj);
AccessibleUnknown *           Accessible_queryInterface       (Accessible *obj,
					           const char *interface_name);

/* AccessibleAction function prototypes  */

void       AccessibleAction_ref            (AccessibleAction *obj);
void       AccessibleAction_unref          (AccessibleAction *obj);
long       AccessibleAction_getNActions    (AccessibleAction *obj);
char      *AccessibleAction_getName        (AccessibleAction *obj,
					    long int          i);
char      *AccessibleAction_getDescription (AccessibleAction *obj,
					    long int          i);
SPIBoolean AccessibleAction_doAction       (AccessibleAction *obj,
					    long int          i);
char      *AccessibleAction_getKeyBinding  (AccessibleAction *obj,
					    long int          i);

/* AccessibleApplication function prototypes  */

void       AccessibleApplication_ref            (AccessibleApplication *obj);
void       AccessibleApplication_unref          (AccessibleApplication *obj);
char      *AccessibleApplication_getToolkitName (AccessibleApplication *obj);
char      *AccessibleApplication_getVersion     (AccessibleApplication *obj);
long       AccessibleApplication_getID          (AccessibleApplication *obj);
char      *AccessibleApplication_getLocale      (AccessibleApplication *obj, int lc_category);
SPIBoolean AccessibleApplication_pause          (AccessibleApplication *obj);
SPIBoolean AccessibleApplication_resume         (AccessibleApplication *obj);

/* AccessibleCollection function prototypes */
void       AccessibleCollection_ref (AccessibleCollection *obj);
void       AccessibleCollection_unref (AccessibleCollection *obj);
AccessibleMatchRule *
AccessibleCollection_createMatchRule (AccessibleCollection *obj,
				      AccessibleStateSet *states,
				      AccessibleCollectionMatchType statematchtype,
				      AccessibleAttributeSet *attributes,
				      AccessibleCollectionMatchType attributematchtype,
				      AccessibleRoleSet *roles,
				      AccessibleCollectionMatchType rolematchtype,
				      char *interfaces,
				      AccessibleCollectionMatchType interfacematchtype,
				      long int invert);
void
AccessibleCollection_freeMatchRule (AccessibleCollection *obj,
				    AccessibleMatchRule  *matchrule);
/* AccessibleComponent function prototypes */

void        AccessibleComponent_ref         (AccessibleComponent *obj);
void        AccessibleComponent_unref       (AccessibleComponent *obj);
SPIBoolean  AccessibleComponent_contains    (AccessibleComponent *obj,
					     long int             x,
					     long int             y,
					     AccessibleCoordType  ctype);
Accessible *AccessibleComponent_getAccessibleAtPoint (
                                             AccessibleComponent *obj,
					     long int             x,
					     long int             y,
					     AccessibleCoordType  ctype);
void        AccessibleComponent_getExtents  (AccessibleComponent *obj,
					     long int            *x,
					     long int            *y,
					     long int            *width,
					     long int            *height,
					     AccessibleCoordType  ctype);
void        AccessibleComponent_getPosition (AccessibleComponent *obj,
					     long int            *x,
					     long int            *y,
					     AccessibleCoordType  ctype);
void        AccessibleComponent_getSize     (AccessibleComponent *obj,
					     long int            *width,
					     long int            *height);
AccessibleComponentLayer
            AccessibleComponent_getLayer    (AccessibleComponent *obj);
SPIBoolean  AccessibleComponent_grabFocus   (AccessibleComponent *obj);
short       AccessibleComponent_getMDIZOrder(AccessibleComponent *obj);
double      AccessibleComponent_getAlpha    (AccessibleComponent *obj);

/* AccessibleDocument function prototypes  */

void        AccessibleDocument_ref               (AccessibleDocument *obj);
void        AccessibleDocument_unref             (AccessibleDocument *obj);
char       *AccessibleDocument_getLocale         (AccessibleDocument *obj);
char       *AccessibleDocument_getAttributeValue (AccessibleDocument *obj,
						  char *attribute);
AccessibleAttributeSet *AccessibleDocument_getAttributes     (AccessibleDocument *obj);

/* AccessibleEditableText function prototypes  */

void
AccessibleEditableText_ref (AccessibleEditableText *obj);

void
AccessibleEditableText_unref (AccessibleEditableText *obj);

SPIBoolean
AccessibleEditableText_setAttributes (AccessibleEditableText *obj,
				      const char *attributes,
				      long int startOffset,
				      long int endOffset);

SPIBoolean
AccessibleEditableText_setTextContents (AccessibleEditableText *obj,
                                        const char *newContents);

SPIBoolean
AccessibleEditableText_insertText (AccessibleEditableText *obj,
                                   long int position,
                                   const char *text,
                                   long int length);

SPIBoolean
AccessibleEditableText_copyText (AccessibleText *obj,
                                 long int startPos,
                                 long int endPos);

SPIBoolean
AccessibleEditableText_cutText (AccessibleEditableText *obj,
                                long int startPos,
                                long int endPos);

SPIBoolean
AccessibleEditableText_deleteText (AccessibleEditableText *obj,
                                   long int startPos,
                                   long int endPos);

SPIBoolean
AccessibleEditableText_pasteText (AccessibleEditableText *obj,
                                  long int position);

/*
 *
 * AccessibleHyperlink function prototypes
 *
 */
void
AccessibleHyperlink_ref (AccessibleHyperlink *obj);
void
AccessibleHyperlink_unref (AccessibleHyperlink *obj);

long
AccessibleHyperlink_getNAnchors (AccessibleHyperlink *obj);

char *
AccessibleHyperlink_getURI (AccessibleHyperlink *obj,
                            long int i);

Accessible *
AccessibleHyperlink_getObject (AccessibleHyperlink *obj,
                               long int i);

void
AccessibleHyperlink_getIndexRange (AccessibleHyperlink *obj,
                                   long int *startIndex,
                                   long int *endIndex);

SPIBoolean
AccessibleHyperlink_isValid (AccessibleHyperlink *obj);

/*
 *
 * AccessibleHypertext function prototypes
 *
 */

void
AccessibleHypertext_ref (AccessibleHypertext *obj);

void
AccessibleHypertext_unref (AccessibleHypertext *obj);

long
AccessibleHypertext_getNLinks (AccessibleHypertext *obj);

AccessibleHyperlink *
AccessibleHypertext_getLink (AccessibleHypertext *obj,
                             long int linkIndex);

long
AccessibleHypertext_getLinkIndex (AccessibleHypertext *obj,
                                  long int characterOffset);

/*
 *
 * AccessibleImage function prototypes
 *
 */

void
AccessibleImage_ref (AccessibleImage *obj);

void
AccessibleImage_unref (AccessibleImage *obj);

char *
AccessibleImage_getImageDescription (AccessibleImage *obj);

void
AccessibleImage_getImageSize (AccessibleImage *obj,
                              long int *width,
                              long int *height);

void
AccessibleImage_getImagePosition (AccessibleImage *obj,
                                  long int *x,
                                  long int *y,
                                  AccessibleCoordType ctype);

void
AccessibleImage_getImageExtents (AccessibleImage *obj,
				 long int *x,
				 long int *y,
				 long int *width,
				 long int *height,
				 AccessibleCoordType ctype);
char *
AccessibleImage_getImageLocale  (AccessibleImage *obj);

/*
 *
 * AccessibleMatchRule function prototypes
 *
 */
void       AccessibleMatchRule_ref (AccessibleMatchRule *obj);
void       AccessibleMatchRule_unref (AccessibleMatchRule *obj);

/*
 *
 * AccessibleRelation function prototypes
 *
 */

void AccessibleRelation_ref   (AccessibleRelation *obj);
void AccessibleRelation_unref (AccessibleRelation *obj);

AccessibleRelationType
AccessibleRelation_getRelationType (AccessibleRelation *obj);

int
AccessibleRelation_getNTargets (AccessibleRelation *obj);

Accessible *
AccessibleRelation_getTarget (AccessibleRelation *obj, int i);


/*
 *
 * AccessibleSelection function prototypes
 *
 */

void AccessibleSelection_ref   (AccessibleSelection *obj);
void AccessibleSelection_unref (AccessibleSelection *obj);

long
AccessibleSelection_getNSelectedChildren (AccessibleSelection *obj);

Accessible *
AccessibleSelection_getSelectedChild (AccessibleSelection *obj,
                                      long int selectedChildIndex);

SPIBoolean
AccessibleSelection_selectChild (AccessibleSelection *obj,
                                 long int childIndex);

SPIBoolean
AccessibleSelection_deselectSelectedChild (AccessibleSelection *obj,
                                           long int selectedChildIndex);

SPIBoolean
AccessibleSelection_deselectChild (AccessibleSelection *obj,
				   long int childIndex);

SPIBoolean
AccessibleSelection_isChildSelected (AccessibleSelection *obj,
                                     long int childIndex);

SPIBoolean
AccessibleSelection_selectAll (AccessibleSelection *obj);

SPIBoolean
AccessibleSelection_clearSelection (AccessibleSelection *obj);


/*
 *
 * AccessibleStateSet function prototypes
 *
 */

void AccessibleStateSet_ref   (AccessibleStateSet *obj);
void AccessibleStateSet_unref (AccessibleStateSet *obj);

SPIBoolean
AccessibleStateSet_contains (AccessibleStateSet *obj,
                             AccessibleState state);

void
AccessibleStateSet_add (AccessibleStateSet *obj,
                        AccessibleState state);

void
AccessibleStateSet_remove (AccessibleStateSet *obj,
                           AccessibleState state);

SPIBoolean
AccessibleStateSet_equals (AccessibleStateSet *obj,
                           AccessibleStateSet *obj2);

AccessibleStateSet *
AccessibleStateSet_compare (AccessibleStateSet *obj,
                            AccessibleStateSet *obj2);

SPIBoolean
AccessibleStateSet_isEmpty (AccessibleStateSet *obj);

void
AccessibleStreamableContent_ref (AccessibleStreamableContent *obj);
void
AccessibleStreamableContent_unref (AccessibleStreamableContent *obj);
char **
AccessibleStreamableContent_getContentTypes (AccessibleStreamableContent *obj);

void
AccessibleStreamableContent_freeContentTypesList (AccessibleStreamableContent *obj,
						 char **content_types);
#define AccessibleStreamableContent_freeContentTypeList(a, b) AccessibleStreamableContent_freeContentTypesList(a,b)

SPIBoolean
AccessibleStreamableContent_open (AccessibleStreamableContent *obj,
				  const char *content_type);
SPIBoolean
AccessibleStreamableContent_close (AccessibleStreamableContent *obj);

long
AccessibleStreamableContent_seek (AccessibleStreamableContent *obj,
				  long int offset,
				  AccessibleStreamableContentSeekType seek_type);
SPIBoolean
AccessibleStreamableContent_read (AccessibleStreamableContent *obj,
				  void *buff,
				  long int nbytes,
				  unsigned int read_type);
/*
 *
 * AccessibleTable function prototypes
 *
 */

void AccessibleTable_ref   (AccessibleTable *obj);
void AccessibleTable_unref (AccessibleTable *obj);

Accessible *
AccessibleTable_getCaption (AccessibleTable *obj);

Accessible *
AccessibleTable_getSummary (AccessibleTable *obj);

long
AccessibleTable_getNRows (AccessibleTable *obj);

long
AccessibleTable_getNColumns (AccessibleTable *obj);

Accessible *
AccessibleTable_getAccessibleAt (AccessibleTable *obj,
                                 long int row,
                                 long int column);

long
AccessibleTable_getIndexAt (AccessibleTable *obj,
                            long int row,
                            long int column);

long
AccessibleTable_getRowAtIndex (AccessibleTable *obj,
                               long int index);

long
AccessibleTable_getColumnAtIndex (AccessibleTable *obj,
                                  long int index);

char *
AccessibleTable_getRowDescription (AccessibleTable *obj,
				   long int row);

char *
AccessibleTable_getColumnDescription (AccessibleTable *obj,
				      long int column);

long
AccessibleTable_getRowExtentAt (AccessibleTable *obj,
                                long int row,
                                long int column);

long
AccessibleTable_getColumnExtentAt (AccessibleTable *obj,
                                   long int row,
                                   long int column);

SPIBoolean
AccessibleTable_getRowColumnExtentsAtIndex (AccessibleTable *obj,
					    long int index, long int *row, long int *col, 
					    long int *row_extents, long int *col_extents, 
					    long int *is_selected);

Accessible *
AccessibleTable_getRowHeader (AccessibleTable *obj,
			      long int row);

Accessible *
AccessibleTable_getColumnHeader (AccessibleTable *obj,
				 long int column);

long
AccessibleTable_getNSelectedRows (AccessibleTable *obj);

long
AccessibleTable_getSelectedRows (AccessibleTable *obj,
                                 long int **selectedRows);

long
AccessibleTable_getNSelectedColumns (AccessibleTable *obj);

long
AccessibleTable_getSelectedColumns (AccessibleTable *obj,
                                    long int **selectedColumns);

SPIBoolean
AccessibleTable_isRowSelected (AccessibleTable *obj,
                               long int row);

SPIBoolean
AccessibleTable_isColumnSelected (AccessibleTable *obj,
                                  long int column);

SPIBoolean
AccessibleTable_isSelected (AccessibleTable *obj,
                            long int row,
                            long int column);

SPIBoolean
AccessibleTable_addRowSelection (AccessibleTable *obj,
				 long int row);
SPIBoolean
AccessibleTable_addColumnSelection (AccessibleTable *obj,
				    long int column);

SPIBoolean
AccessibleTable_removeRowSelection (AccessibleTable *obj,
				    long int row);
SPIBoolean
AccessibleTable_removeColumnSelection (AccessibleTable *obj,
				       long int column);

/*
 *
 * AccessibleText function prototypes
 *
 */

void AccessibleText_ref   (AccessibleText *obj);
void AccessibleText_unref (AccessibleText *obj);

long
AccessibleText_getCharacterCount (AccessibleText *obj);

char *
AccessibleText_getText (AccessibleText *obj,
                        long int startOffset,
                        long int endOffset);

long
AccessibleText_getCaretOffset (AccessibleText *obj);

char *
AccessibleText_getAttributes (AccessibleText *obj,
				 long int offset,
				 long int *startOffset,
				 long int *endOffset);

char *
AccessibleText_getDefaultAttributes (AccessibleText *obj);

SPIBoolean
AccessibleText_setCaretOffset (AccessibleText *obj,
                               long int newOffset);

char *
AccessibleText_getTextBeforeOffset (AccessibleText *obj,
                                    long int offset,
                                    AccessibleTextBoundaryType type,
				    long int *startOffset,
				    long int *endOffset);

char *
AccessibleText_getTextAtOffset (AccessibleText *obj,
				long int offset,
				AccessibleTextBoundaryType type,
				long int *startOffset,
				long int *endOffset);

char *
AccessibleText_getTextAfterOffset (AccessibleText *obj,
				   long int offset,
				   AccessibleTextBoundaryType type,
				   long int *startOffset,
				   long int *endOffset);

unsigned long
AccessibleText_getCharacterAtOffset (AccessibleText *obj,
                                     long int offset);

void
AccessibleText_getCharacterExtents (AccessibleText *obj,
                                    long int offset,
                                    long int *x,
                                    long int *y,
                                    long int *width,
                                    long int *height,
				    AccessibleCoordType type);

void
AccessibleText_getRangeExtents (AccessibleText *obj,
				long int startOffset,
				long int endOffset,
				long int *x,
				long int *y,
				long int *width,
				long int *height,
				AccessibleCoordType type);

AccessibleTextRange **
AccessibleText_getBoundedRanges (AccessibleText *obj,
				 long int x,
				 long int y,
				 long int width,
				 long int height,
				 AccessibleCoordType type,
				 AccessibleTextClipType clipTypeX,
				 AccessibleTextClipType clipTypeY);

void
AccessibleTextRange_freeRanges (AccessibleTextRange **ranges);

long
AccessibleText_getOffsetAtPoint (AccessibleText *obj,
                                 long int x,
                                 long int y,
				 AccessibleCoordType type);

long
AccessibleText_getNSelections (AccessibleText *obj);

void
AccessibleText_getSelection (AccessibleText *obj,
			     long int selectionNum,
			     long int *startOffset,
			     long int *endOffset);


SPIBoolean
AccessibleText_addSelection (AccessibleText *obj,
			     long int startOffset,
			     long int endOffset);

SPIBoolean
AccessibleText_removeSelection (AccessibleText *obj,
				long int selectionNum);

SPIBoolean
AccessibleText_setSelection (AccessibleText *obj,
			     long int selectionNum,
			     long int startOffset,
			     long int endOffset);
AccessibleAttributeSet *
AccessibleText_getAttributeRun  (AccessibleText *obj,
				 long int offset,
				 long int *startOffset,
				 long int *endOffset,
				 long int includeDefaults);
AccessibleAttributeSet *
AccessibleText_getDefaultAttributeSet (AccessibleText *obj);

/* AccessibleValue Function Prototypes:  */

void       AccessibleValue_ref             (AccessibleValue *obj);
void       AccessibleValue_unref           (AccessibleValue *obj);
double     AccessibleValue_getMinimumValue (AccessibleValue *obj);
double     AccessibleValue_getCurrentValue (AccessibleValue *obj);
double     AccessibleValue_getMaximumValue (AccessibleValue *obj);
SPIBoolean AccessibleValue_setCurrentValue (AccessibleValue *obj,
					    double           newValue);
double     AccessibleValue_getMinimumIncrement (AccessibleValue *obj);

/* Persistance and lifecycle control for AccessibleEvents. */
SPIBoolean AccessibleEvent_ref (const AccessibleEvent *e);
void AccessibleEvent_unref (const AccessibleEvent *e);

/*
 * Prototypes for accessor functions, to obtain context
 * information for accessible events.
 */

char*                  AccessibleEvent_getSourceName (const AccessibleEvent *e);
AccessibleRole         AccessibleEvent_getSourceRole (const AccessibleEvent *e);
AccessibleApplication* AccessibleEvent_getSourceApplication (const AccessibleEvent *e);
SPIBoolean             AccessibleEvent_getSourceDetails (const AccessibleEvent *e, char **name, 
							 AccessibleRole *role, 
							 AccessibleApplication **app);

char*        AccessibleTextChangedEvent_getChangeString (const AccessibleEvent *e);
Accessible * AccessibleChildChangedEvent_getChildAccessible (const AccessibleEvent *e);

Accessible * AccessibleParentChangedEvent_getParentAccessible (const AccessibleEvent *e);

char*        AccessibleTextSelectionChangedEvent_getSelectionString (const AccessibleEvent *e);

char*        AccessibleWindowEvent_getTitleString (const AccessibleEvent *e);

Accessible * AccessibleActiveDescendantChangedEvent_getActiveDescendant (const AccessibleEvent *e);

Accessible * AccessibleTableSummaryChangedEvent_getSummaryAccessible (const AccessibleEvent *e); 

Accessible * AccessibleTableHeaderChangedEvent_getHeaderAccessible (const AccessibleEvent *e);

char *       AccessibleTableCaptionChangedEvent_getCaptionString (const AccessibleEvent *e);

char *       AccessibleTableRowDescriptionChangedEvent_getDescriptionString (const AccessibleEvent *e);

char *       AccessibleTableColumnDescriptionChangedEvent_getDescriptionString (const AccessibleEvent *e);

char *       AccessibleDescriptionChangedEvent_getDescriptionString (const AccessibleEvent *e);

char *       AccessibleNameChangedEvent_getNameString (const AccessibleEvent *e);
SPIRect *    AccessibleBoundsChangedEvent_getNewBounds (const AccessibleEvent *e);

/* Misc methods and error handling */
void SPI_freeString (char *s);

char* SPI_dupString (char *s);

void SPI_freeRect (SPIRect *r);

SPIBoolean SPI_exceptionHandlerPush (SPIExceptionHandler *handler);

SPIExceptionHandler* SPI_exceptionHandlerPop (void);

SPIExceptionType SPIException_getSourceType (SPIException *err);

SPIExceptionCode SPIException_getExceptionCode (SPIException *err);

Accessible* SPIAccessibleException_getSource (SPIException *err);

char* SPIException_getDescription (SPIException *err);

#ifdef  __cplusplus
}
#endif

#endif
