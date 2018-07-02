// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**************************************************************************

Monitor commands
Dxxxx,yyyy      = Dump memory
Fxxxx,yyyy,zz   = Fill memory
Gxxxx           = Goto
Ixx             = In port
Lxxxx           = Load
Mxxxx,yyyy,zzzz = Move x-y to z
Oxx,yy          = Out port
-               = Edit memory
.               = Edit memory


    TODO:

    - all

****************************************************************************/

#include "emu.h"
#include "bus/rs232/rs232.h"
//#include "bus/s100/s100.h"
#include "cpu/z80/z80.h"
#include "machine/z80daisy.h"
#include "machine/am9519.h"
#include "machine/com8116.h"
#include "machine/ram.h"
#include "machine/z80dart.h"
#include "machine/z80pio.h"

#define Z80_TAG         "u45"
#define Z80DART_0_TAG   "u14"
#define Z80DART_1_TAG   "u30"
#define Z80PIO_TAG      "u43"
#define AM9519_TAG      "u13"
#define BR1941_TAG      "u12"
#define RS232_A_TAG     "rs232a"
#define RS232_B_TAG     "rs232b"
#define RS232_C_TAG     "rs232c"
#define RS232_D_TAG     "rs232d"

class superslave_state : public driver_device
{
public:
	superslave_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, Z80_TAG)
		, m_dbrg(*this, BR1941_TAG)
		, m_ram(*this, RAM_TAG)
		, m_rs232a(*this, RS232_A_TAG)
		, m_rs232b(*this, RS232_B_TAG)
		, m_rs232c(*this, RS232_C_TAG)
		, m_rs232d(*this, RS232_D_TAG)
		, m_rom(*this, Z80_TAG)
		, m_memctrl(0x01)
		, m_cmd(0x01)
	{ }

	void superslave(machine_config &config);

private:
	virtual void machine_start() override;
	virtual void machine_reset() override;
	void superslave_io(address_map &map);
	void superslave_mem(address_map &map);

	DECLARE_READ8_MEMBER( read );
	DECLARE_WRITE8_MEMBER( write );
	DECLARE_WRITE8_MEMBER( memctrl_w );
	DECLARE_READ8_MEMBER( status_r );
	DECLARE_WRITE8_MEMBER( cmd_w );

	required_device<cpu_device> m_maincpu;
	required_device<com8116_device> m_dbrg;
	required_device<ram_device> m_ram;
	required_device<rs232_port_device> m_rs232a;
	required_device<rs232_port_device> m_rs232b;
	required_device<rs232_port_device> m_rs232c;
	required_device<rs232_port_device> m_rs232d;
	required_memory_region m_rom;

	uint8_t m_memctrl;
	uint8_t m_cmd;
};



//**************************************************************************
//  READ/WRITE HANDLERS
//**************************************************************************

//-------------------------------------------------
//  read -
//-------------------------------------------------

READ8_MEMBER( superslave_state::read )
{
	uint8_t data = 0;

	offs_t boundary = 0xc000 | ((m_memctrl & 0xf0) << 6);

	if ((offset < 0x1000) && BIT(m_cmd, 0))
	{
		data = m_rom->base()[offset & 0x7ff];
	}
	else if (offset < boundary)
	{
		if (BIT(m_memctrl, 0))
		{
			data = m_ram->pointer()[offset];
		}
		else if (BIT(m_memctrl, 1) && (m_ram->size() > 0x10000))
		{
			data = m_ram->pointer()[0x10000 | offset];
		}
	}
	else
	{
		data = m_ram->pointer()[offset];
	}

	return data;
}


//-------------------------------------------------
//  write -
//-------------------------------------------------

WRITE8_MEMBER( superslave_state::write )
{
	offs_t boundary = 0xc000 | ((m_memctrl & 0xf0) << 6);

	if (offset < boundary)
	{
		if (BIT(m_memctrl, 0))
		{
			m_ram->pointer()[offset] = data;
		}
		else if (BIT(m_memctrl, 1) && (m_ram->size() > 0x10000))
		{
			m_ram->pointer()[0x10000 | offset] = data;
		}
	}
	else
	{
		m_ram->pointer()[offset] = data;
	}
}


//-------------------------------------------------
//  memctrl_w -
//-------------------------------------------------

WRITE8_MEMBER( superslave_state::memctrl_w )
{
	/*

	    bit     description

	    0       Memory bank 0 on
	    1       Memory bank 1 on
	    2
	    3
	    4       Unswitched memory boundary bit 0
	    5       Unswitched memory boundary bit 1
	    6       Unswitched memory boundary bit 2
	    7       Unswitched memory boundary bit 3

	*/

	m_memctrl = data;
}


//-------------------------------------------------
//  status_r -
//-------------------------------------------------

READ8_MEMBER( superslave_state::status_r)
{
	/*

	    bit     description

	    0       Sense Switch (0=closed)
	    1       Parity error (1=error)
	    2       Syncerr (1=error)
	    3       Service Request
	    4       Data Set Ready 3
	    5       Data Set Ready 2
	    6       Data Set Ready 1
	    7       Data Set Ready 0

	*/

	uint8_t data = 1;

	// RS-232
	data |= m_rs232d->dsr_r() << 4;
	data |= m_rs232c->dsr_r() << 5;
	data |= m_rs232b->dsr_r() << 6;
	data |= m_rs232a->dsr_r() << 7;

	return data;
}


//-------------------------------------------------
//  cmd_w -
//-------------------------------------------------

WRITE8_MEMBER( superslave_state::cmd_w )
{
	/*

	    bit     description

	    0       Prom enable (1=enabled)
	    1       Clear Parity (1=cleared)
	    2       Clear Syncerr (1=cleared)
	    3       Service Request
	    4       Wait Protocol (0=Wait)
	    5       Command bit 5
	    6       Command bit 6
	    7       Command bit 7

	*/

	m_cmd = data;
}



//**************************************************************************
//  ADDRESS MAPS
//**************************************************************************

//-------------------------------------------------
//  ADDRESS_MAP( superslave_mem )
//-------------------------------------------------

void superslave_state::superslave_mem(address_map &map)
{
	map(0x0000, 0xffff).rw(FUNC(superslave_state::read), FUNC(superslave_state::write));
}


//-------------------------------------------------
//  ADDRESS_MAP( superslave_io )
//-------------------------------------------------

void superslave_state::superslave_io(address_map &map)
{
	map.global_mask(0xff);
	map(0x00, 0x03).rw(Z80DART_0_TAG, FUNC(z80dart_device::ba_cd_r), FUNC(z80dart_device::ba_cd_w));
	map(0x0c, 0x0f).rw(Z80DART_1_TAG, FUNC(z80dart_device::ba_cd_r), FUNC(z80dart_device::ba_cd_w));
	map(0x10, 0x10).mirror(0x03).w(BR1941_TAG, FUNC(com8116_device::stt_str_w));
	map(0x14, 0x17).rw(Z80PIO_TAG, FUNC(z80pio_device::read_alt), FUNC(z80pio_device::write_alt));
	map(0x18, 0x18).mirror(0x02).rw(AM9519_TAG, FUNC(am9519_device::data_r), FUNC(am9519_device::data_w));
	map(0x19, 0x19).mirror(0x02).rw(AM9519_TAG, FUNC(am9519_device::stat_r), FUNC(am9519_device::cmd_w));
	map(0x1d, 0x1d).w(FUNC(superslave_state::memctrl_w));
	map(0x1e, 0x1e).noprw(); // master communications
	map(0x1f, 0x1f).rw(FUNC(superslave_state::status_r), FUNC(superslave_state::cmd_w));
}



//**************************************************************************
//  INPUT PORTS
//**************************************************************************

//-------------------------------------------------
//  INPUT_PORTS( superslave )
//-------------------------------------------------

static INPUT_PORTS_START( superslave )
	PORT_START("SW1")
	PORT_DIPNAME( 0x7f, 0x38, "Slave Address" ) PORT_DIPLOCATION("SW1:1,2,3,4,5,6,7")
	PORT_DIPSETTING(    0x38, "70H (0)" )
	PORT_DIPSETTING(    0x39, "72H (1)" )
	PORT_DIPSETTING(    0x3a, "74H (2)" )
	PORT_DIPSETTING(    0x3b, "76H (3)" )
	PORT_DIPSETTING(    0x3c, "78H (4)" )
	PORT_DIPSETTING(    0x3d, "7AH (5)" )
	PORT_DIPSETTING(    0x3e, "7CH (6)" )
	PORT_DIPSETTING(    0x3f, "7EH (7)" )
	PORT_DIPSETTING(    0x40, "80H (8)" )
	PORT_DIPSETTING(    0x41, "82H (9)" )
	PORT_DIPSETTING(    0x42, "84H (10)" )
	PORT_DIPSETTING(    0x43, "86H (11)" )
	PORT_DIPSETTING(    0x44, "88H (12)" )
	PORT_DIPSETTING(    0x45, "8AH (13)" )
	PORT_DIPSETTING(    0x46, "8CH (14)" )
	PORT_DIPSETTING(    0x47, "8EH (15)" )
INPUT_PORTS_END



//**************************************************************************
//  DEVICE CONFIGURATION
//**************************************************************************

static DEVICE_INPUT_DEFAULTS_START( terminal )
	DEVICE_INPUT_DEFAULTS( "RS232_TXBAUD", 0xff, RS232_BAUD_9600 )
	DEVICE_INPUT_DEFAULTS( "RS232_RXBAUD", 0xff, RS232_BAUD_9600 )
	DEVICE_INPUT_DEFAULTS( "RS232_STARTBITS", 0xff, RS232_STARTBITS_1 )
	DEVICE_INPUT_DEFAULTS( "RS232_DATABITS", 0xff, RS232_DATABITS_8 )
	DEVICE_INPUT_DEFAULTS( "RS232_PARITY", 0xff, RS232_PARITY_NONE )
	DEVICE_INPUT_DEFAULTS( "RS232_STOPBITS", 0xff, RS232_STOPBITS_1 )
DEVICE_INPUT_DEFAULTS_END


//-------------------------------------------------
//  z80_daisy_config superslave_daisy_chain
//-------------------------------------------------

static const z80_daisy_config superslave_daisy_chain[] =
{
	{ Z80DART_0_TAG },
	{ Z80DART_1_TAG },
	{ Z80PIO_TAG },
	{ nullptr }
};



//**************************************************************************
//  MACHINE INITIALIZATION
//**************************************************************************

//-------------------------------------------------
//  machine_start()
//-------------------------------------------------

void superslave_state::machine_start()
{
	// state saving
	save_item(NAME(m_memctrl));
	save_item(NAME(m_cmd));
}


void superslave_state::machine_reset()
{
	m_memctrl = 0x01;
	m_cmd = 0x01;
}



//**************************************************************************
//  MACHINE DRIVERS
//**************************************************************************

//-------------------------------------------------
//  MACHINE_CONFIG( superslave )
//-------------------------------------------------

MACHINE_CONFIG_START(superslave_state::superslave)
	// basic machine hardware
	MCFG_DEVICE_ADD(Z80_TAG, Z80, XTAL(8'000'000)/2)
	MCFG_DEVICE_PROGRAM_MAP(superslave_mem)
	MCFG_DEVICE_IO_MAP(superslave_io)
	MCFG_Z80_DAISY_CHAIN(superslave_daisy_chain)

	// devices
	MCFG_DEVICE_ADD(AM9519_TAG, AM9519, 0)
	MCFG_AM9519_OUT_INT_CB(INPUTLINE(Z80_TAG, INPUT_LINE_IRQ0))

	MCFG_DEVICE_ADD(Z80PIO_TAG, Z80PIO, XTAL(8'000'000)/2)
	MCFG_Z80PIO_OUT_INT_CB(INPUTLINE(Z80_TAG, INPUT_LINE_IRQ0))

	MCFG_DEVICE_ADD(Z80DART_0_TAG, Z80DART, XTAL(8'000'000)/2)
	MCFG_Z80DART_OUT_TXDA_CB(WRITELINE(RS232_A_TAG, rs232_port_device, write_txd))
	MCFG_Z80DART_OUT_DTRA_CB(WRITELINE(RS232_A_TAG, rs232_port_device, write_dtr))
	MCFG_Z80DART_OUT_RTSA_CB(WRITELINE(RS232_A_TAG, rs232_port_device, write_rts))
	MCFG_Z80DART_OUT_TXDB_CB(WRITELINE(RS232_B_TAG, rs232_port_device, write_txd))
	MCFG_Z80DART_OUT_DTRB_CB(WRITELINE(RS232_B_TAG, rs232_port_device, write_dtr))
	MCFG_Z80DART_OUT_RTSB_CB(WRITELINE(RS232_B_TAG, rs232_port_device, write_rts))
	MCFG_Z80DART_OUT_INT_CB(INPUTLINE(Z80_TAG, INPUT_LINE_IRQ0))

	MCFG_DEVICE_ADD(RS232_A_TAG, RS232_PORT, default_rs232_devices, "terminal")
	MCFG_RS232_RXD_HANDLER(WRITELINE(Z80DART_0_TAG, z80dart_device, rxa_w))
	MCFG_RS232_DCD_HANDLER(WRITELINE(Z80DART_0_TAG, z80dart_device, dcda_w))
	MCFG_RS232_CTS_HANDLER(WRITELINE(Z80DART_0_TAG, z80dart_device, ctsa_w))
	MCFG_SLOT_OPTION_DEVICE_INPUT_DEFAULTS("terminal", terminal)

	MCFG_DEVICE_ADD(RS232_B_TAG, RS232_PORT, default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(WRITELINE(Z80DART_0_TAG, z80dart_device, rxb_w))
	MCFG_RS232_DCD_HANDLER(WRITELINE(Z80DART_0_TAG, z80dart_device, dcdb_w))
	MCFG_RS232_CTS_HANDLER(WRITELINE(Z80DART_0_TAG, z80dart_device, ctsb_w))

	MCFG_DEVICE_ADD(Z80DART_1_TAG, Z80DART, XTAL(8'000'000)/2)
	MCFG_Z80DART_OUT_TXDA_CB(WRITELINE(RS232_C_TAG, rs232_port_device, write_txd))
	MCFG_Z80DART_OUT_DTRA_CB(WRITELINE(RS232_C_TAG, rs232_port_device, write_dtr))
	MCFG_Z80DART_OUT_RTSA_CB(WRITELINE(RS232_C_TAG, rs232_port_device, write_rts))
	MCFG_Z80DART_OUT_TXDB_CB(WRITELINE(RS232_D_TAG, rs232_port_device, write_txd))
	MCFG_Z80DART_OUT_DTRB_CB(WRITELINE(RS232_D_TAG, rs232_port_device, write_dtr))
	MCFG_Z80DART_OUT_RTSB_CB(WRITELINE(RS232_D_TAG, rs232_port_device, write_rts))
	MCFG_Z80DART_OUT_INT_CB(INPUTLINE(Z80_TAG, INPUT_LINE_IRQ0))

	MCFG_DEVICE_ADD(RS232_C_TAG, RS232_PORT, default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(WRITELINE(Z80DART_1_TAG, z80dart_device, rxa_w))
	MCFG_RS232_DCD_HANDLER(WRITELINE(Z80DART_1_TAG, z80dart_device, dcda_w))
	MCFG_RS232_CTS_HANDLER(WRITELINE(Z80DART_1_TAG, z80dart_device, ctsa_w))

	MCFG_DEVICE_ADD(RS232_D_TAG, RS232_PORT, default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(WRITELINE(Z80DART_1_TAG, z80dart_device, rxb_w))
	MCFG_RS232_DCD_HANDLER(WRITELINE(Z80DART_1_TAG, z80dart_device, dcdb_w))
	MCFG_RS232_CTS_HANDLER(WRITELINE(Z80DART_1_TAG, z80dart_device, ctsb_w))

	MCFG_DEVICE_ADD(BR1941_TAG, COM8116, XTAL(5'068'800))
	MCFG_COM8116_FR_HANDLER(WRITELINE(Z80DART_0_TAG, z80dart_device, txca_w))
	MCFG_DEVCB_CHAIN_OUTPUT(WRITELINE(Z80DART_0_TAG, z80dart_device, rxca_w))
	MCFG_DEVCB_CHAIN_OUTPUT(WRITELINE(Z80DART_1_TAG, z80dart_device, txca_w))
	MCFG_DEVCB_CHAIN_OUTPUT(WRITELINE(Z80DART_1_TAG, z80dart_device, rxca_w))
	MCFG_COM8116_FT_HANDLER(WRITELINE(Z80DART_0_TAG, z80dart_device, rxtxcb_w))
	MCFG_DEVCB_CHAIN_OUTPUT(WRITELINE(Z80DART_1_TAG, z80dart_device, rxtxcb_w))

	// internal ram
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("64K")
	MCFG_RAM_EXTRA_OPTIONS("128K")
MACHINE_CONFIG_END



//**************************************************************************
//  ROMS
//**************************************************************************

//-------------------------------------------------
//  ROM( superslv )
//-------------------------------------------------

ROM_START( superslv )
	ROM_REGION( 0x800, Z80_TAG, 0 )
	ROM_LOAD( "adcs6_slave_v3.2.bin", 0x000, 0x800, CRC(7f39322d) SHA1(2e9621e09378a1bb6fc05317bb58ae7865e52744) )
ROM_END



//**************************************************************************
//  SYSTEM DRIVERS
//**************************************************************************

//    YEAR  NAME      PARENT  COMPAT  MACHINE     INPUT       CLASS             INIT        COMPANY                         FULLNAME        FLAGS
COMP( 1983, superslv, 0,      0,      superslave, superslave, superslave_state, empty_init, "Advanced Digital Corporation", "Super Slave",  MACHINE_NOT_WORKING | MACHINE_NO_SOUND_HW )
