// license:BSD-3-Clause
// copyright-holders:Joakim Larsson Edstrom
/*
 * vme.cpp
 *
 * The Versabus-E was standardized as the VME bus by VITA 1981 for Europe
 * in the single or double Euroboard form factor. Several standard revs has
 * been approved since then up until recently and the VME64 revision.
 *
 * This bus driver starts with Versabus and VME rev C.
 *   http://bitsavers.informatik.uni-stuttgart.de/pdf/motorola/versabus/M68KVBS_VERSAbus_Specification_Manual_Jul81.pdf
 *
 * Acronymes from the specification
 * ---------------------------------
 * BACKPLANE  - A printed circuit board which provides the interconnection path
                between other printed circuit cards.
   SLOT       - A single position at which a card may be inserted into the backplane.
                One slot may consist of more than one edge connector.
   BOARD/CARD - Interchangeable terms representing one printed circuit board capable
                of being inserted into the backplane and containing a collection of
                electronic components.
   MODULE     - A collection of electronic components with a single functional
                purpose. More than one module may exist on the same card, but one
                module should never be spread over multiple cards.
   MASTER     - A functional module capable of initiating data bus transfers.
   REQUESTER  - A functional module capable of requesting control of the data
                transfer bus.
   INTERRUPT  - A functional module capable of detecting interrupt requests
   HANDLER      and initiating appropriate responses.
   MASTER     - The combination of a MASTER, REQUESTER, INTERRUPT HANDLER, and
   SUB-SYSTEM   (optionally) an INTERRUPTER, which function together and which
                must be on the same card.

   NOTE! All MASTERS, REQUESTERS, and INTERRUPT HANDLERS must be pieces of a
         MASTER SUB-SYSTEM.

   SLAVE       - A functional module capable of responding to data transfer
                 operations generated by a MASTER.
   INTERRUPTER - A functional module capable of requesting service from a MASTER
                 SUB-SYSTEM by generating an interrupt request.
   SLAVE       - The combination of a SLAVE and INTERRUPTER which function together
   SUB-SYSTEM    and which must be on the same card.

   NOTE! All INTERRUPTERS must be part of either SLAVE SUB-SYSTEMS or MASTER
         SUB-SYSTEMS. However, SLAVES may exist as stand-alone elements.
         Such SLAVES will never be called SLAVE SUB-SYSTEMS.

   CONTROLLER  - The combination of modules used to provide utility and emergency
   SUB-SYSTEM    signals for the VERSAbus. There will always be one and only one
                 CONTROLLER SUB-SYSTEM. It can contain the following functional
                 modules:

                 a. Data Transfer Bus ARBITER
                 b. Emergency Data Transfer Bus REQUESTER
                 c. Power up/power down MASTER
                 d. System clock driver
                 e. System reset driver
                 f. System test controller
                 g. Power monitor (for AC clock and AC fail driver)

    In any VERSAbus system, only one each of the above functional modules will exist.
    The slot numbered Al is designated as the controller sub-system slot because the
    user will typically provide modules a through d on the board residing in this
    slot. System reset and the system test  controller are typically connected to
    an operator control panel and may be located elsewhere. The power monitor is
    interfaced to the incoming AC power source and may also be located remotely.
*/

#include "emu.h"
#include "vme.h"
//#include "bus/vme/vme_mzr8105.h"
#include "bus/vme/vme_mzr8300.h"
#include "bus/vme/vme_mvme350.h"
#include "bus/vme/vme_fcisio.h"
#include "bus/vme/vme_fcscsi.h"

#define LOG_GENERAL 0x01
#define LOG_SETUP   0x02
#define LOG_PRINTF  0x04

#define VERBOSE 0 // (LOG_PRINTF | LOG_SETUP  | LOG_GENERAL)

#define LOGMASK(mask, ...)   do { if (VERBOSE & mask) logerror(__VA_ARGS__); } while (0)
#define LOGLEVEL(mask, level, ...) do { if ((VERBOSE & mask) >= level) logerror(__VA_ARGS__); } while (0)

#define LOG(...)      LOGMASK(LOG_GENERAL, __VA_ARGS__)
#define LOGSETUP(...) LOGMASK(LOG_SETUP,   __VA_ARGS__)

#define logerror printf // logerror is not available here

#ifdef _MSC_VER
#define FUNCNAME __func__
#else
#define FUNCNAME __PRETTY_FUNCTION__
#endif

//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

const device_type VME_SLOT = &device_creator<vme_slot_device>;

//-------------------------------------------------
//  vme_slot_device - constructor
//-------------------------------------------------
vme_slot_device::vme_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
		device_t(mconfig, VME_SLOT, "VME_SLOT", tag, owner, clock, "vme_slot", __FILE__)
		,device_slot_interface(mconfig, *this)
		,m_vme_tag(nullptr)
		,m_vme_slottag(nullptr)
		,m_vme_j1_callback(*this)
{
	LOG("%s %s\n", tag, FUNCNAME);
}

vme_slot_device::vme_slot_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, uint32_t clock, const char *shortname, const char *source) :
		device_t(mconfig, type, name, tag, owner, clock, shortname, source),
		device_slot_interface(mconfig, *this)
		,m_vme_tag(nullptr)
		,m_vme_slottag(nullptr)
		,m_vme_j1_callback(*this)
{
	LOG("%s %s\n", tag, FUNCNAME);
}

//-------------------------------------------------
//  static_update_vme_chains
//-------------------------------------------------
void vme_slot_device::static_update_vme_chains(device_t &device, uint32_t slot_nbr)
{
	LOG("%s %s - %d\n", FUNCNAME, device.tag(), slot_nbr);
}

//-------------------------------------------------
//  static_set_vme_slot
//-------------------------------------------------
void vme_slot_device::static_set_vme_slot(device_t &device, const char *tag, const char *slottag)
{
	LOG("%s %s - %s\n", FUNCNAME, tag, slottag);
	vme_slot_device &vme_card = dynamic_cast<vme_slot_device&>(device);
	vme_card.m_vme_tag = tag;
	vme_card.m_vme_slottag = slottag;
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------
void vme_slot_device::device_start()
{
	device_vme_card_interface *dev = dynamic_cast<device_vme_card_interface *>(get_card_device());
	LOG("%s %s - %s:%s\n", tag(), FUNCNAME, m_vme_tag, m_vme_slottag);
	if (dev)
	{
		device_vme_card_interface::static_set_vme_tag(*dev, m_vme_tag, m_vme_slottag);
	}

	//  m_card = dynamic_cast<device_vme_card_interface *>(get_card_device());
}

//-------------------------------------------------
//  device_config_complete - perform any
//  operations now that the configuration is
//  complete
//-------------------------------------------------
void vme_slot_device::device_config_complete()
{
	LOG("%s %s\n", tag(), FUNCNAME);
}

//-------------------------------------------------
//  P1 D8 read
//-------------------------------------------------
READ8_MEMBER(vme_slot_device::read8)
{
	uint16_t result = 0x00;
	LOG("%s %s\n", tag(), FUNCNAME);
	//  printf("%s %s\n", tag(), FUNCNAME);
	//  if (m_card)     result = m_card->read8(space, offset);
	return result;
}

//-------------------------------------------------
//  P1 D8 write
//-------------------------------------------------
WRITE8_MEMBER(vme_slot_device::write8)
{
	LOG("%s %s\n", tag(), FUNCNAME);
	//  printf("%s %s\n", tag(), FUNCNAME);
	//  if (m_card)     m_card->write8(space, offset, data);
}

#if 0 // Disabled until we know how to make a board driver also a slot device
/* The following two slot collections be combined once we intriduce capabilities for each board */
/* Usually a VME firmware supports only a few boards so it will have its own slot collection defined */
// Controller capable boards that can go into slot1 ( or has an embedded VME bus )
SLOT_INTERFACE_START( vme_slot1 )
//  SLOT_INTERFACE("mzr8105", VME_MZR8105)
SLOT_INTERFACE_END
#endif

// All boards that can be non-controller boards, eg not driving the VME CLK etc
SLOT_INTERFACE_START( vme_slots )
	SLOT_INTERFACE("mzr8300", VME_MZR8300)
	SLOT_INTERFACE("mvme350", VME_MVME350)
	SLOT_INTERFACE("fcisio1", VME_FCISIO1)
	SLOT_INTERFACE("fcscsi1", VME_FCSCSI1)
SLOT_INTERFACE_END

//
// VME device P1
//

const device_type VME = &device_creator<vme_device>;

// static_set_cputag - used to be able to lookup the CPU owning this VME bus
void vme_device::static_set_cputag(device_t &device, const char *tag)
{
	vme_device &vme = downcast<vme_device &>(device);
	vme.m_cputag = tag;
}

// static_set_use_owner_spaces - disables use of the memory interface and use the address spaces 
// of the owner instead. This is useful for VME buses where no address modifiers or arbitration is
// being used and gives some gain in performance. 
void vme_device::static_use_owner_spaces(device_t &device)
{
	LOG("%s %s\n", device.tag(), FUNCNAME);
	vme_device &vme = downcast<vme_device &>(device);

	vme.m_allocspaces = false;
}

vme_device::vme_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, VME, "VME", tag, owner, clock, "vme", __FILE__)
	, device_memory_interface(mconfig, *this)
	, m_a32_config("VME A32", ENDIANNESS_BIG, 32, 32, 0, nullptr)
	, m_allocspaces(true)
	, m_cputag("maincpu")
{
	LOG("%s %s\n", tag, FUNCNAME);
}

vme_device::vme_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, uint32_t clock, const char *shortname, const char *source) :
	device_t(mconfig, type, name, tag, owner, clock, shortname, source)
	, device_memory_interface(mconfig, *this)
	, m_a32_config("VME A32", ENDIANNESS_BIG, 32, 32, 0, nullptr)
	, m_allocspaces(true)
	, m_cputag("maincpu")
{
	LOG("%s %s\n", tag, FUNCNAME);
}

vme_device::~vme_device()
{
	LOG("%s %s\n", tag(), FUNCNAME);
	m_device_list.detach_all();
}

void vme_device::device_start()
{
	LOG("%s %s %s\n", owner()->tag(), tag(), FUNCNAME);
	if (m_allocspaces)
	{
		LOG(" - using my own memory spaces\n");
		m_prgspace = &space(AS_PROGRAM);
		m_prgwidth = m_prgspace->data_width();
		LOG(" - Done at %d width\n", m_prgwidth);
	}
	else    // use host CPU's spaces directly
	{
		LOG(" - using owner memory spaces for %s\n", m_cputag);
		m_maincpu = owner()->subdevice<cpu_device>(m_cputag);
		m_prgspace = &m_maincpu->space(AS_PROGRAM);
		m_prgwidth = m_maincpu->space_config(AS_PROGRAM)->m_databus_width;
		LOG(" - Done at %d width\n", m_prgwidth);
	}
}

void vme_device::device_reset()
{
	LOG("%s %s\n", tag(), FUNCNAME);
}

void vme_device::add_vme_card(device_vme_card_interface *card)
{
	LOG("%s %s\n", tag(), FUNCNAME);
	m_device_list.append(*card);
}

#if 0
/* 
 *  Install UB (Utility Bus) handlers for this board
 * 
 * The Utility Bus signal lines
 *------------------------------
 * System Clock (SYSCLK)
 * Serial Clock (SERCLK)
 * Serial Data (SERDAT*)
 * AC Fail (ACFAIL*)
 * System Reset (SYSRESET*)
 * System Failure (SYSFAIL*)
 *------------------------------
 */
void vme_device::install_ub_handler(offs_t start, offs_t end, read8_delegate rhandler, write8_delegate whandler, uint32_t mask)
{
}
#endif

/* 
 *  Install DTB (Data Transfer Bus) handlers for this board
 */

// D8 bit devices in A16, A24 and A32
void vme_device::install_device(vme_amod_t amod, offs_t start, offs_t end, read8_delegate rhandler, write8_delegate whandler, uint32_t mask)
{
	LOG("%s %s AM%d D%02x\n", tag(), FUNCNAME, amod, m_prgwidth);

	LOG(" - width:%d\n", m_prgwidth);

	// TODO: support address modifiers and buscycles other than single access cycles
	switch(amod)
	{
	case A16_SC: break;
	case A24_SC: break;
	case A32_SC: break;
	default: fatalerror("VME D8: Non supported Address modifier: AM%02x\n", amod);
	}

	switch(m_prgwidth)
	{
	case 16:
		m_prgspace->install_readwrite_handler(start, end, rhandler, whandler, (uint16_t)(mask & 0x0000ffff));
		break;
	case 24: 
		m_prgspace->install_readwrite_handler(start, end, rhandler, whandler, (uint32_t)(mask & 0x00ffffff));
		break;
	case 32:
		m_prgspace->install_readwrite_handler(start, end, rhandler, whandler, mask); 
		break;
	default: fatalerror("VME D8: Bus width %d not supported\n", m_prgwidth);
	}
}

// D16 bit devices in A16, A24 and A32
void vme_device::install_device(vme_amod_t amod, offs_t start, offs_t end, read16_delegate rhandler, write16_delegate whandler, uint32_t mask)
{
	LOG("%s %s AM%d D%02x\n", tag(), FUNCNAME, amod, m_prgwidth);

	LOG(" - width:%d\n", m_prgwidth);

	// TODO: support address modifiers and buscycles other than single access cycles
	switch(amod)
	{
	case A16_SC: break;
	case A24_SC: break;
	case A32_SC: break;
	default: fatalerror("VME D16: Non supported Address modifier: %02x\n", amod);
	}

	switch(m_prgwidth)
	{
	case 16:
		m_prgspace->install_readwrite_handler(start, end, rhandler, whandler, (uint16_t)(mask & 0x0000ffff));
		break;
	case 24: 
		m_prgspace->install_readwrite_handler(start, end, rhandler, whandler, (uint32_t)(mask & 0x00ffffff));
		break;
	case 32:
		m_prgspace->install_readwrite_handler(start, end, rhandler, whandler, mask); 
		break;
	default: fatalerror("VME D16: Bus width %d not supported\n", m_prgwidth);
	}
}

// D32 bit devices in A16, A24 and A32
void vme_device::install_device(vme_amod_t amod, offs_t start, offs_t end, read32_delegate rhandler, write32_delegate whandler, uint32_t mask)
{
	LOG("%s %s AM%d D%02x\n", tag(), FUNCNAME, amod, m_prgwidth);

	LOG(" - width:%d\n", m_prgwidth);

	// TODO: support address modifiers and buscycles other than single access cycles
	switch(amod)
	{
	case A16_SC: break;
	case A24_SC: break;
	case A32_SC: break;
	default: fatalerror("VME D32: Non supported Address modifier: %02x\n", amod);
	}

	switch(m_prgwidth)
	{
	case 16:
		m_prgspace->install_readwrite_handler(start, end, rhandler, whandler, (uint16_t)(mask & 0x0000ffff));
		break;
	case 24: 
		m_prgspace->install_readwrite_handler(start, end, rhandler, whandler, (uint32_t)(mask & 0x00ffffff));
		break;
	case 32:
		m_prgspace->install_readwrite_handler(start, end, rhandler, whandler, mask); 
		break;
	default: fatalerror("VME D32: Bus width %d not supported\n", m_prgwidth);
	}
}

//
// Card interface
//
device_vme_card_interface::device_vme_card_interface(const machine_config &mconfig, device_t &device)
	: device_slot_card_interface(mconfig, device)
	,m_vme(nullptr)
	,m_vme_tag(nullptr)
	,m_vme_slottag(nullptr)
	,m_slot(0)
	,m_next(nullptr)
{
	m_device = &device;
	LOG("%s %s\n", m_device->tag(), FUNCNAME);
}

device_vme_card_interface::~device_vme_card_interface()
{
	LOG("%s %s\n", m_device->tag(), FUNCNAME);
}

void device_vme_card_interface::static_set_vme_tag(device_t &device, const char *tag, const char *slottag)
{
	LOG("%s %s\n", tag, FUNCNAME);
	device_vme_card_interface &vme_card = dynamic_cast<device_vme_card_interface &>(device);
	vme_card.m_vme_tag = tag;
	vme_card.m_vme_slottag = slottag;
}

void device_vme_card_interface::set_vme_device()
{
	LOG("%s %s\n", m_device->tag(), FUNCNAME);
	m_vme = dynamic_cast<vme_device *>(device().machine().device(m_vme_tag));
	//  printf("*** %s %sfound\n", m_vme_tag, m_vme ? "" : "not ");
	if (m_vme) m_vme->add_vme_card(this);
}

/* VME D8 accesses */
READ8_MEMBER(device_vme_card_interface::read8)
{
	uint8_t result = 0x00;
	LOG("%s %s Offset:%08x\n", m_device->tag(), FUNCNAME, offset);
	return result;
}

WRITE8_MEMBER(device_vme_card_interface::write8)
{
	LOG("%s %s Offset:%08x\n", m_device->tag(), FUNCNAME, offset);
}

//--------------- P2 connector below--------------------------
/*
The VME P2 connector only specifies the mid row B of the connector
and leaves row A and C to be system specific. This has resulted in
a number of variants that has been more or less standardized:

- VMXbus was available on the first VME boards but not standardized hence
an almost compatible variant was developed by Motorola called MVMX32bus.
- VSBbus replaced VMX and MVMX32and was approved by IEEE in 1988
- SCSA is a P2 standardization for telephony voice and facsimile applications
- SkyChannel is packet switched P2 architecture from Sky Computers and standardized
through VITA/VSO.
- RACEway is a 40Mhz P2 bus allowing 480MBps throughput from Mercusry Computers and
standardized through VITA/VSO.
- VME64 adds two more rows, called 'z' and 'd', of user defined pins to the P2 connector
- P2CI adds a PCI bus onto a VME64 P2 connector

URL:s
http://rab.ict.pwr.wroc.pl/dydaktyka/supwa/vme/secbuses.html
http://www.interfacebus.com/Design_Connector_VME_P2_Buses.html

TODO: Figure out a good way to let all these variants coexist and interconnect in a VME system

*/
