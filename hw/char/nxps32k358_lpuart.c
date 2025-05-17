#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/char/nxps32k358_lpuart.h"
#include "hw/qdev-properties.h"
#include "chardev/char.h" // Per qemu_chr_fe_*

// prova per il commit
cc
// PRIMA REVISIONE 17 MAY -19.35
static void nxps32k358_lpuart_update_irq(NXPS32K358LpuartState *s) {
    bool irq_pending = false;

    // Controlla l'interrupt di ricezione dati
    if ((s->lpuart_cr & LPUART_CTRL_RIE) && (s->lpuart_sr & LPUART_STAT_RDRF)) {
        irq_pending = true;
    }

    // Controlla l'interrupt di trasmettitore pronto
    if (!irq_pending && (s->lpuart_cr & LPUART_CTRL_TIE) && (s->lpuart_sr & LPUART_STAT_TDRE)) {
        // Spesso TIE e RIE sono mutuamente esclusivi o gestiti con priorità.
        // Se RDRF è attivo, potresti non voler segnalare TDRE subito,
        // ma questo dipende dalla logica esatta del chip.
        // Per semplicità, qui li consideriamo indipendenti se non già irq_pending.
        // In molti casi, è un OR di tutte le condizioni.
        irq_pending = true;
    }
    // La riga sopra può essere semplificata se tutti gli interrupt possono essere attivi contemporaneamente:
    // if ((s->lpuart_cr & LPUART_CTRL_TIE) && (s->lpuart_sr & LPUART_STAT_TDRE)) {
    //     irq_pending = true;
    // }
    // Controlla l'interrupt di trasmissione completata
    if ((s->lpuart_cr & LPUART_CTRL_TCIE) && (s->lpuart_sr & LPUART_STAT_TC)) {
        irq_pending = true;
    }

    // AGGIUNGI QUI I CONTROLLI PER GLI INTERRUPT DI ERRORE (OR, NF, FE, PF)
    // Esempio per Overrun (ORIE e OR sono nomi ipotetici, verifica sul manuale S32K3):
    // #define LPUART_CTRL_ORIE (1 << XYZ) // Bit di enable per interrupt Overrun
    // #define LPUART_STAT_OR   (1 << ABC) // Flag di stato Overrun
    // if ((s->lpuart_cr & LPUART_CTRL_ORIE) && (s->lpuart_sr & LPUART_STAT_OR)) {
    //     irq_pending = true;
    // }

    // AGGIUNGI QUI I CONTROLLI PER ALTRI INTERRUPT SPECIFICI (IDLE, LIN Break, ecc.)


    // Infine, imposta lo stato della linea IRQ
    qemu_set_irq(s->irq, irq_pending ? 1 : 0);
}

// Funzione chiamata quando QEMU può inviare un carattere al guest
static int nxps32k358_lpuart_can_receive(void *opaque) {
    NXPS32K358LpuartState *s = NXPS32K358_LPUART(opaque);

    // Controlla se c'è spazio nella FIFO di ricezione o se il ricevitore è abilitato
    if (!(s->lpuart_cr & LPUART_CTRL_RE) || s->rx_fifo_len >= sizeof(s->rx_fifo)) {
        return 0; // Non può ricevere
    }
    return 1; // Può ricevere
}

//----- OK 17 May 18.55

// Funzione chiamata quando QEMU ha un carattere da inviare al guest
static void nxps32k358_lpuart_receive(void *opaque, const uint8_t *buf, int size) {
    NXPS32K358LpuartState *s = NXPS32K358_LPUART(opaque);

    // return when the size is 0(so no data to be sent) or when the RE is not enabled
    if (!(s->lpuart_cr & LPUART_CTRL_RE) || size == 0) {
        return;
    }

    // Metti il carattere nella FIFO di ricezione (semplificato)
    // In un'implementazione reale, gestisci overflow, errori di framing/parità se emulati.
    if (s->rx_fifo_len < sizeof(s->rx_fifo)) {
        s->rx_fifo[s->rx_fifo_pos + s->rx_fifo_len] = buf[0]; // Prende solo il primo carattere per semplicità
        s->rx_fifo_len++;
        s->lpuart_sr |= LPUART_STAT_RDRF; // Imposta Receive Data Register Full
        s->lpuart_sr &= ~LPUART_STAT_TDRE; // Potrebbe essere necessario per la logica (TDRE è per TX)
                                         // Questo è un esempio, la logica di STAT deve essere accurata.

        qemu_log_mask(LOG_GUEST_ERROR, "LPUART RX: %02x ('%c')\n", buf[0], isprint(buf[0]) ? buf[0] : '.');
    } else {
        qemu_log_mask(LOG_GUEST_ERROR, "LPUART RX FIFO overflow\n");
        // Imposta un flag di errore di overflow in STAT se la LPUART lo prevede
    }

    // at the end need to be done to send the Interrupt :)
    nxps32k358_lpuart_update_irq(s);
}

// ----- Ok 17 May 19.05 da capire ancora come sistemare la fifo

static void nxps32k358_lpuart_reset(DeviceState *dev) {
    NXPS32K358LpuartState *s = NXPS32K358_LPUART(dev);

    s->lpuart_cr = 0x00000000; // Valore di reset dal manuale
    s->lpuart_sr = 0x00C00000; // TDRE è solitamente 1 al reset, TBD
                                 // Altri flag (es. TC) potrebbero essere 1. Verifica!
    s->baud_rate_config = 0x0F000004; // Valore di reset dal manuale (esempio)
    s->fifo_reg = 0x00C00011; // Valore di reset dal manuale (esempio, se FIFO è implementato)

    s->rx_fifo_pos = 0;
    s->rx_fifo_len = 0;
    s->tx_char_pending = false;
    nxps32k358_lpuart_update_irq(s);
}


// ----- Ok 17 May 19.10 

// hwaddr is called offset because it the offset to access to a given register. In this case is possible because MemoryRegionOps è
// l'offset relativo a quella memor region definita. Sarà opaque a definire la lpuart

static uint64_t nxps32k358_lpuart_read(void *opaque, hwaddr offset, unsigned size) {
    NXPS32K358LpuartState *s = NXPS32K358_LPUART(opaque);
    uint32_t ret = 0;

    // qemu_log_mask(LOG_GUEST_ERROR, "LPUART read offset=0x%02x size=%u\n", (int)offset, size);

    switch (offset) {
        case LPUART_BAUD:
            ret = s->baud_rate_config;
            break;
        case LPUART_STAT:
            ret = s->lpuart_sr;
            // Alcuni bit di stato (come RDRF) potrebbero essere read-to-clear o auto-clearing
            // dopo la lettura del registro DATA. Implementa questa logica se necessario.
            break;
        case LPUART_CTRL:
            ret = s->lpuart_cr;
            break;
        case LPUART_DATA:
            if (s->rx_fifo_len > 0) {
                ret = s->rx_fifo[s->rx_fifo_pos];
                s->rx_fifo_pos++;
                s->rx_fifo_len--;
                if (s->rx_fifo_len == 0) {
                    s->rx_fifo_pos = 0;
                    s->lpuart_sr &= ~LPUART_STAT_RDRF; // Clear Receive Data Register Full
                }
                qemu_log_mask(LOG_GUEST_ERROR, "LPUART read DATA: %02x ('%c')\n", ret, isprint(ret) ? ret : '.');
            } else {
                ret = 0; // FIFO vuota
                qemu_log_mask(LOG_GUEST_ERROR, "LPUART read DATA: RX FIFO empty\n");
            }
            // La lettura di DATA spesso cancella RDRF e l'interrupt associato
            nxps32k358_lpuart_update_irq(s);
            break;
        // Aggiungi qui i case per altri registri leggibili (VERID, PARAM, FIFO, WATER, ecc.)
        // basandoti sul S32K3XXRM.pdf
        case LPUART_VERID: ret = 0x04010003; break; // Valore esempio, metti quello reale
        case LPUART_PARAM: ret = 0x00000202; break; // Valore esempio
        case LPUART_FIFO:
            // ret = (s->tx_fifo_count << X) | (s->rx_fifo_count << Y); // Esempio
            break;
        default:
            qemu_log_mask(LOG_UNIMP, "NXP S32K3 LPUART: Unimplemented read offset 0x%" HWADDR_PRIx "\n", offset);
            ret = 0;
            break;
    }
    return ret;
}

static void nxps32k358_lpuart_write(void *opaque, hwaddr offset, uint64_t value, unsigned size) {
    NXPS32K358LpuartState *s = NXPS32K358_LPUART(opaque);

    // qemu_log_mask(LOG_GUEST_ERROR, "LPUART write offset=0x%02x val=0x%08x size=%u\n", (int)offset, (uint32_t)value, size);

    switch (offset) {
        case LPUART_BAUD:
            s->baud_rate_config = value;
            // Qui dovresti ricalcolare/riconfigurare il baud rate effettivo se lo emuli
            // qemu_log_mask(LOG_GUEST_ERROR, "LPUART BAUD set to 0x%08x\n", (uint32_t)value);
            break;
        case LPUART_STAT:
            // Alcuni bit di STAT sono write-1-to-clear.
            // Esempio: s->lpuart_sr &= ~(value & (LPUART_STAT_SOME_W1C_FLAG));
            // Verifica sul manuale quali bit sono W1C.
            // qemu_log_mask(LOG_GUEST_ERROR, "LPUART STAT write 0x%08x (W1C?)\n", (uint32_t)value);
            break;
        case LPUART_CTRL:
            s->lpuart_cr = value;
            // Se TE o RE vengono disabilitati/abilitati, aggiorna lo stato
            if (!(s->lpuart_cr & LPUART_CTRL_TE)) {
                s->tx_char_pending = false; // Cancella eventuale trasmissione pendente
            }
            if (!(s->lpuart_cr & LPUART_CTRL_RE)) {
                s->rx_fifo_len = 0; // Svuota la FIFO di ricezione
                s->rx_fifo_pos = 0;
                s->lpuart_sr &= ~LPUART_STAT_RDRF;
            }
            // qemu_log_mask(LOG_GUEST_ERROR, "LPUART CTRL set to 0x%08x\n", (uint32_t)value);
            break;
        case LPUART_DATA:
            if (s->lpuart_cr & LPUART_CTRL_TE) { // Transmitter enabled
                s->tx_char = (uint8_t)value;
                s->tx_char_pending = true;
                s->lpuart_sr &= ~LPUART_STAT_TDRE; // Non più vuoto, in attesa di trasmissione
                // qemu_log_mask(LOG_GUEST_ERROR, "LPUART write DATA: %02x ('%c')\n", s->tx_char, isprint(s->tx_char) ? s->tx_char : '.');
                nxps32k358_lpuart_transmit(s); // Tenta di trasmettere subito
            }
            break;
        // Aggiungi qui i case per altri registri scrivibili (PINCFG, MODIR, FIFO, WATER, ecc.)
        // basandoti sul S32K3XXRM.pdf
        case LPUART_FIFO:
            // Gestisci abilitazione/flush delle FIFO (es. TXFE, RXFE, TXFLUSH, RXFLUSH)
            // s->fifo_reg = value;
            break;
        default:
            qemu_log_mask(LOG_UNIMP, "NXP S32K3 LPUART: Unimplemented write offset 0x%" HWADDR_PRIx " val 0x%" PRIx64 "\n", offset, value);
            break;
    }
    nxps32k358_lpuart_update_irq(s);
}


// Ok 17 May 19.31

// Funzione per inviare un carattere dal guest all'host (QEMU)
static void nxps32k358_lpuart_transmit(NXPS32K358LpuartState *s) {
    if (s->tx_char_pending && (s->lpuart_cr & LPUART_CTRL_TE)) {
        if (qemu_chr_fe_write(&s->chr, &s->tx_char, 1) == 1) {
            s->tx_char_pending = false;
            s->lpuart_sr |= LPUART_STAT_TDRE; // Transmit Data Register Empty
            // Potrebbe essere necessario un interrupt di "Transmit Complete" o "TX FIFO Empty"
            nxps32k358_lpuart_update_irq(s);
             qemu_log_mask(LOG_GUEST_ERROR, "LPUART TX: %02x ('%c')\n", s->tx_char, isprint(s->tx_char) ? s->tx_char : '.');
        } else {
            // Il backend non può accettare il carattere ora, riprova più tardi
            // Questo richiede una gestione più complessa con timer o attesa che il backend sia pronto
             qemu_log_mask(LOG_GUEST_ERROR, "LPUART TX: backend busy for %02x\n", s->tx_char);
        }
    }
}

// ok 17 May 19.32 da ricontrollare per gestire il timer

// Funzioni finali che vanno bene cosi come sono

static const MemoryRegionOps nxps32k358_lpuart_ops = {
    .read = nxps32k358_lpuart_read,
    .write = nxps32k358_lpuart_write,
    .endianness = DEVICE_LITTLE_ENDIAN, // Verifica l'endianness delle periferiche S32K3
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4, // Solitamente accessi a 8, 16, 32 bit
    },
};



static void nxps32k358_lpuart_init(Object *obj) {
    NXPS32K358LpuartState *s = NXPS32K358_LPUART(obj);

    // Inizializza la MemoryRegion per i registri della LPUART
    // La dimensione (es. 0x1000 o 4KB) deve coprire tutti i registri LPUART
    memory_region_init_io(&s->iomem, obj, &nxps32k358_lpuart_ops, s,
                          "nxps32k358-lpuart", 0x1000); // Dimensione esempio
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);

    // Inizializza la linea IRQ
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    // Inizializza il backend per i caratteri
    qdev_prop_set_chr(DEVICE(obj), "chardev", &s->chr); // Permette di specificare -serial ...
                                                      // o un altro chardev
}

static void nxps32k358_lpuart_realize(DeviceState *dev, Error **errp) {
    NXPS32K358LpuartState *s = NXPS32K358_LPUART(dev);

    // Connetti le funzioni di callback per la ricezione dei caratteri
    // dall'host QEMU al dispositivo emulato.
    qemu_chr_fe_set_handlers(&s->chr, nxps32k358_lpuart_can_receive,
                             nxps32k358_lpuart_receive, NULL, NULL, s, NULL, true);
    
    // Qui non serve connettere clock perché il clock viene passato dal SoC
    // al momento della connessione in nxps32k358_soc_realize
    // qdev_connect_clock_in(dev, "clk", some_clock_source);
}


static Property nxps32k358_lpuart_properties[] = {
    DEFINE_PROP_CHR("chardev", NXPS32K358LpuartState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void nxps32k358_lpuart_class_init(ObjectClass *klass, void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = nxps32k358_lpuart_reset;
    dc->realize = nxps32k358_lpuart_realize;
    // dc->vmsd = &vmstate_nxps32k358_lpuart; // Per il salvataggio/ripristino dello stato VM
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories); // È un dispositivo di input/char
    device_class_set_props(dc, nxps32k358_lpuart_properties);
}

static const TypeInfo nxps32k358_lpuart_info = {
    .name = TYPE_NXPS32K358_LPUART,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(NXPS32K358LpuartState),
    .instance_init = nxps32k358_lpuart_init,
    .class_init = nxps32k358_lpuart_class_init,
};

static void nxps32k358_lpuart_register_types(void) {
    type_register_static(&nxps32k358_lpuart_info);
}

type_init(nxps32k358_lpuart_register_types);
