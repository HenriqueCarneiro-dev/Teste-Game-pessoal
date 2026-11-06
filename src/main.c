#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W 1280
#define H 720
#define N 5
#define NE 27
#define FASES 5
#define DIAS 4
#define DIA_SEG 30.0f
#define SEC_DIAS 3
#define SEC_DIA_SEG 40.0f
#define BAT_MAX 110.0f
#define LIM_BAIXO 30.0f
#define LIM_ALTO 80.0f
#define NUM_EV 9
#define MAX_EV 5
#define MAX_RANK 10
#define TAM_NOME 12
#define NUM_RANK 6      /* 5 fases + secreta */
#define CEL 3
#define GW (W / CEL + 1)
#define GH (H / CEL + 1)
#define SR 22050
#define TEMPO_CRONO 20.0f
#define NUM_MUS 7       /* 0 menu, 1..5 fases, 6 secreta */
#define COMBO_SEG 12.0f
#define COMBO_MAX 5

enum { NORTE, NORDESTE, CENTRO, SUDESTE, SUL };
typedef enum { TELA_MENU, TELA_AJUDA, TELA_FASES, TELA_RANK, TELA_JOGO, TELA_MINI, TELA_PAUSA,
               TELA_TROCA, TELA_NOME, TELA_FIM } Tela;
typedef enum { MINI_FIOS, MINI_DISJUNTOR, MINI_GERADOR,
               MINI_TURBINAS, MINI_SINCRONIA, MINI_ALVOS } TipoMini;

typedef struct {
    const char *nome;
    Color cor;
    int fonte;
    Vector2 pt[56];
    int n;
    Vector2 c;
    float bat, ajuste, consBase;
    float multProd, multCons;
    bool ativa;
} Regiao;

typedef struct {
    const char *nome, *sig;
    int reg;
    Vector2 c;
    float bat, ajuste, cons;
} Estado;

typedef struct {
    const char *nome;
    float prod[N];
    float cons[N];
} EvDef;

typedef struct {
    int id;
    float t, dur;
} EvAtivo;

/* No da lista encadeada do ranking */
typedef struct No {
    char nome[16];
    int pontos;
    struct No *prox;
} No;

typedef struct {
    TipoMini tipo;
    bool blackout;
    int reg;
    float tempo, tempoMax;
    int ordem[4];
    int destino[4];
    int arrasta;
    int slot[6];
    bool on[6];
    int proximo;
    float flash;
    float calor[4], taxa[4];
    float marcador, dir, zona, larg, vel;
    int acertos;
    float carga;
    int alvo;
    float alvoT;
    int resultado;   /* 0 jogando, 1 venceu, 2 perdeu */
    float fimT;
} Minigame;

/* Definicao de uma trilha musical */
typedef struct {
    int raiz[16];
    int tipo[16];    /* 0 = m7, 1 = maj7, 2 = dominante (7) */
    float bpm;
    int arp;
    int prob;
    unsigned seed;
    float vol;
} Trilha;

/* ---------- Dados ---------- */

static const char *abrev[N] = {"Norte", "Nordeste", "C.Oeste", "Sudeste", "Sul"};
static const int ordemFase[N] = {SUDESTE, NORDESTE, CENTRO, SUL, NORTE};
static const Color fios[4] = {
    {235, 70, 70, 255}, {70, 140, 255, 255}, {255, 220, 50, 255}, {65, 220, 110, 255}
};

/* Matriz de vizinhanca entre regioes (1 = fazem fronteira) */
static const int vizinho[N][N] = {
    /* NORTE NORDESTE CENTRO SUDESTE SUL */
    {  0,      1,      1,     0,      0 },   /* Norte    */
    {  1,      0,      1,     1,      0 },   /* Nordeste */
    {  1,      1,      0,     1,      1 },   /* C.Oeste  */
    {  0,      1,      1,     0,      1 },   /* Sudeste  */
    {  0,      0,      1,     1,      0 }    /* Sul      */
};

static const EvDef evDefs[NUM_EV] = {
    {"Chuvas fortes",      {0.30f, 0.00f,-0.10f, 0.12f, 0.28f}, {0.00f, 0.08f, 0.00f, 0.00f, 0.00f}},
    {"Seca severa",       {-0.40f, 0.10f, 0.00f,-0.25f,-0.30f}, {0.00f, 0.00f, 0.10f, 0.00f, 0.00f}},
    {"Onda de calor",     { 0.00f, 0.12f, 0.30f, 0.00f, 0.00f}, {0.05f, 0.30f, 0.20f, 0.28f, 0.10f}},
    {"Ventos fortes",     { 0.00f, 0.45f, 0.00f, 0.00f, 0.20f}, {0.08f, 0.00f, 0.00f, 0.00f, 0.00f}},
    {"Frente fria",       { 0.00f,-0.15f, 0.00f, 0.00f, 0.00f}, {0.08f, 0.00f, 0.00f, 0.20f, 0.38f}},
    {"Jogo decisivo",     { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f}, {0.00f, 0.20f, 0.00f, 0.40f, 0.10f}},
    {"Manutencao nuclear",{ 0.00f, 0.00f, 0.00f,-0.45f, 0.00f}, {0.00f, 0.00f, 0.00f, 0.00f, 0.00f}},
    {"Sol forte",         { 0.00f, 0.05f, 0.35f, 0.05f, 0.10f}, {0.00f, 0.00f, 0.00f, 0.00f, 0.00f}},
    {"Pico industrial",   { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f}, {0.00f, 0.10f, 0.12f, 0.35f, 0.25f}}
};

/* Trilhas: 0 menu, 1 a 5 = fases, 6 = fase secreta */
static const Trilha trilhas[NUM_MUS] = {
    /* menu: lento e relaxante (Am - F - C - G ...) */
    {{57, 53, 48, 55,  57, 53, 48, 52,  50, 53, 48, 55,  57, 53, 50, 52},
     { 0,  1,  1,  2,   0,  1,  1,  0,   0,  1,  1,  2,   0,  1,  0,  2},
     58.0f, 2, 45, 7u, 0.075f},
    /* fase 1: leve, em Do maior */
    {{48, 45, 53, 55,  48, 45, 53, 55,  48, 52, 53, 55,  48, 45, 53, 55},
     { 1,  0,  1,  2,   1,  0,  1,  2,   1,  0,  1,  2,   1,  0,  1,  2},
     66.0f, 2, 40, 11u, 0.065f},
    /* fase 2: Re menor */
    {{50, 46, 53, 48,  50, 46, 53, 57,  50, 55, 46, 48,  50, 46, 53, 57},
     { 0,  1,  1,  2,   0,  1,  1,  2,   0,  2,  1,  2,   0,  1,  1,  2},
     76.0f, 2, 42, 23u, 0.065f},
    /* fase 3: Mi menor */
    {{52, 48, 55, 50,  52, 48, 55, 59,  52, 57, 48, 50,  52, 48, 50, 59},
     { 0,  1,  1,  2,   0,  1,  1,  2,   0,  0,  1,  2,   0,  1,  2,  2},
     88.0f, 2, 45, 37u, 0.065f},
    /* fase 4: Sol menor */
    {{55, 51, 58, 53,  55, 51, 58, 50,  55, 60, 51, 53,  55, 51, 53, 50},
     { 0,  1,  1,  2,   0,  1,  1,  2,   0,  0,  1,  2,   0,  1,  2,  2},
     100.0f, 2, 48, 51u, 0.06f},
    /* fase 5: La menor, mais tensa */
    {{57, 53, 56, 52,  57, 53, 50, 52,  57, 55, 53, 52,  57, 56, 52, 57},
     { 0,  1,  2,  2,   0,  1,  0,  2,   0,  1,  1,  2,   0,  2,  2,  0},
     112.0f, 2, 50, 67u, 0.06f},
    /* secreta: tom brilhante (Fa lidio) */
    {{53, 55, 50, 57,  53, 55, 52, 57,  53, 57, 50, 55,  53, 55, 52, 53},
     { 1,  2,  0,  0,   1,  2,  0,  0,   1,  0,  0,  2,   1,  2,  0,  1},
     80.0f, 2, 50, 83u, 0.07f}
};

/* Estados (posicoes aproximadas): nome, sigla, regiao, lon, lat */
static const struct { const char *nome, *sig; int reg; float lon, lat; } estDef[NE] = {
    {"Acre","AC",NORTE,-70.5f,-9.0f},        {"Amazonas","AM",NORTE,-64.5f,-4.0f},
    {"Roraima","RR",NORTE,-61.0f,2.0f},      {"Rondonia","RO",NORTE,-63.0f,-11.0f},
    {"Amapa","AP",NORTE,-52.0f,1.5f},        {"Para","PA",NORTE,-52.5f,-4.5f},
    {"Tocantins","TO",NORTE,-48.3f,-10.2f},
    {"Maranhao","MA",NORDESTE,-45.0f,-5.0f}, {"Piaui","PI",NORDESTE,-42.5f,-7.5f},
    {"Ceara","CE",NORDESTE,-39.5f,-5.2f},    {"Rio G. do Norte","RN",NORDESTE,-36.5f,-5.8f},
    {"Paraiba","PB",NORDESTE,-36.5f,-7.2f},  {"Pernambuco","PE",NORDESTE,-38.0f,-8.4f},
    {"Alagoas","AL",NORDESTE,-36.3f,-9.5f},  {"Sergipe","SE",NORDESTE,-37.3f,-10.7f},
    {"Bahia","BA",NORDESTE,-41.7f,-12.5f},
    {"Mato Grosso","MT",CENTRO,-55.5f,-13.0f},{"Mato G. do Sul","MS",CENTRO,-54.5f,-20.5f},
    {"Goias","GO",CENTRO,-49.5f,-15.8f},     {"Distrito Federal","DF",CENTRO,-47.9f,-15.8f},
    {"Minas Gerais","MG",SUDESTE,-44.5f,-18.5f},{"Espirito Santo","ES",SUDESTE,-40.7f,-19.6f},
    {"Rio de Janeiro","RJ",SUDESTE,-42.8f,-22.2f},{"Sao Paulo","SP",SUDESTE,-48.5f,-22.2f},
    {"Parana","PR",SUL,-51.8f,-24.6f},       {"Santa Catarina","SC",SUL,-50.5f,-27.2f},
    {"Rio G. do Sul","RS",SUL,-53.5f,-29.8f}
};

/* Contornos baseados em coordenadas geograficas aproximadas (matrizes lon/lat) */
static const float geoNorte[][2] = {
    {-73.8f,-7.4f},{-72.8f,-5.2f},{-70.1f,-4.2f},{-70.0f,-2.0f},{-69.4f,-0.8f},{-69.6f,0.7f},
    {-67.3f,2.2f},{-65.4f,1.5f},{-64.0f,4.1f},{-61.5f,4.5f},{-60.7f,5.2f},{-59.9f,4.0f},
    {-60.0f,2.0f},{-58.5f,1.3f},{-56.5f,1.9f},{-54.6f,2.3f},{-52.9f,2.2f},{-51.7f,4.2f},
    {-50.8f,2.0f},{-50.0f,0.6f},{-50.6f,-0.4f},{-49.0f,-1.2f},{-47.3f,-0.9f},{-46.2f,-1.1f},
    {-46.8f,-3.0f},{-48.3f,-5.2f},{-47.7f,-5.8f},{-46.6f,-8.5f},{-46.1f,-10.5f},{-46.3f,-12.5f},
    {-50.2f,-13.4f},{-50.5f,-11.0f},{-50.2f,-9.2f},{-52.0f,-9.0f},{-55.0f,-7.5f},{-58.0f,-8.8f},
    {-60.0f,-10.0f},{-60.5f,-13.5f},{-62.5f,-13.0f},{-65.3f,-9.8f},{-67.5f,-10.3f},
    {-70.5f,-11.0f},{-72.5f,-9.5f}
};
static const float geoNordeste[][2] = {
    {-46.2f,-1.1f},{-46.8f,-3.0f},{-48.3f,-5.2f},{-47.7f,-5.8f},{-46.6f,-8.5f},{-46.1f,-10.5f},
    {-46.3f,-12.5f},{-46.0f,-14.0f},{-44.2f,-15.5f},{-43.5f,-16.5f},{-41.8f,-17.9f},{-39.7f,-18.3f},
    {-39.2f,-16.5f},{-38.9f,-15.0f},{-38.5f,-13.0f},{-37.3f,-11.0f},{-35.8f,-9.8f},{-35.2f,-8.5f},
    {-34.85f,-7.2f},{-34.9f,-6.0f},{-35.3f,-5.2f},{-36.5f,-5.1f},{-37.2f,-4.8f},{-38.5f,-3.7f},
    {-40.0f,-2.9f},{-41.5f,-2.9f},{-42.8f,-2.7f},{-44.0f,-2.5f},{-44.8f,-1.6f}
};
static const float geoCentro[][2] = {
    {-60.5f,-13.5f},{-60.0f,-10.0f},{-58.0f,-8.8f},{-55.0f,-7.5f},{-52.0f,-9.0f},{-50.2f,-9.2f},
    {-50.5f,-11.0f},{-50.2f,-13.4f},{-46.3f,-12.5f},{-46.0f,-14.0f},{-46.8f,-16.0f},{-47.2f,-17.0f},
    {-48.2f,-18.3f},{-50.0f,-19.4f},{-51.0f,-20.2f},{-51.9f,-21.2f},{-52.9f,-22.4f},{-53.1f,-22.7f},
    {-54.6f,-24.0f},{-55.4f,-23.7f},{-55.8f,-22.2f},{-57.9f,-22.0f},{-57.7f,-20.0f},{-58.2f,-17.8f},
    {-60.2f,-16.3f},{-60.2f,-15.0f}
};
static const float geoSudeste[][2] = {
    {-46.0f,-14.0f},{-44.2f,-15.5f},{-43.5f,-16.5f},{-41.8f,-17.9f},{-39.7f,-18.3f},{-39.8f,-19.8f},
    {-40.9f,-21.0f},{-41.0f,-22.0f},{-42.0f,-22.9f},{-44.0f,-23.0f},{-45.4f,-23.8f},{-47.0f,-24.8f},
    {-48.1f,-25.0f},{-49.5f,-24.2f},{-51.5f,-22.9f},{-53.1f,-22.7f},{-52.9f,-22.4f},{-51.9f,-21.2f},
    {-51.0f,-20.2f},{-50.0f,-19.4f},{-48.2f,-18.3f},{-47.2f,-17.0f},{-46.8f,-16.0f}
};
static const float geoSul[][2] = {
    {-48.1f,-25.0f},{-49.5f,-24.2f},{-51.5f,-22.9f},{-53.1f,-22.7f},{-54.6f,-24.0f},{-54.6f,-25.5f},
    {-53.8f,-26.2f},{-54.2f,-27.3f},{-56.0f,-28.5f},{-57.6f,-30.2f},{-56.0f,-30.6f},{-54.6f,-31.5f},
    {-53.6f,-32.9f},{-53.4f,-33.7f},{-52.2f,-32.2f},{-50.8f,-30.5f},{-49.7f,-29.0f},{-48.8f,-27.8f},
    {-48.6f,-26.3f}
};

/* ---------- Estado global ---------- */

static Regiao r[N];
static Estado est[NE];
static EvAtivo evs[MAX_EV];
static int nev = 0;
static Tela tela = TELA_MENU;
static Minigame mini;
static int fase = 1, dia = 1, selecionada = SUDESTE, selEst = 23, arrasta = -1;
static int vidas = 3, vidasMax = 3, pontos = 0;
static float tempoDia = 0, tempoTotal = 0, spawnT = 10, cooldown = 0, msgT = 0, pontosAcum = 0;
static float fluxoT = 0;
static int fluxoO = 0, fluxoD = 0;
static char msg[200] = "";
static char tituloMsg[100] = "";
static int corMsg = 0;
static char toast[100] = "";
static float toastT = 0;
static bool vitoria = false, sair = false;
static bool secreta = false, secretaVenceu = false, segredoOk = false;
static bool podeSecreta = false;
static No *rankingLista[NUM_RANK];   /* uma lista encadeada por fase (5) + secreta */
static int rankAtual = 0;
static int abaRank = 0;
static int posRanking = 0;
static char nomeJog[16] = "";
static int nomeLen = 0;
static int paginaAjuda = 0;
static int bonusFase = 0;
static bool introJogo = true;
static short gridEst[GH][GW];        /* matriz: estado dono de cada celula do mapa (-1 = nenhum) */
static RenderTexture2D telaVirtual;
static Rectangle viewport;
static bool painelConfigAberto = false;
static bool telaCheia = false;

/* Bonus de pontuacao (combo) */
static int combo = 1;
static float comboT = 0;

/* Alertas e audio */
static bool alertaPrev[NE];
static float tickT = 0;
static bool audioOk = false;
static float volMus = 0.50f;     /* volume da musica (0 a 1) */
static float volSfx = 0.80f;     /* volume dos efeitos (0 a 1) */
static int arrastaVol = -1;      /* 0 = barra da musica, 1 = barra dos efeitos */
static int musAtual = -1;
static Sound sndClique, sndBlack, sndSobre, sndEvento, sndTick, sndOk, sndFalha, sndApoio;
static Sound mus[NUM_MUS];
static bool musPronta[NUM_MUS];

/* Botoes */
static const Rectangle btnNovo       = {540, 285, 200, 50};
static const Rectangle btnFases      = {540, 343, 200, 50};
static const Rectangle btnRanque     = {540, 401, 200, 50};
static const Rectangle btnComo       = {540, 459, 200, 50};
static const Rectangle btnSair       = {540, 517, 200, 50};
static const Rectangle btnAnt        = {330, 640, 170, 44};
static const Rectangle btnProx       = {780, 640, 170, 44};
static const Rectangle btnVoltar     = {555, 640, 170, 44};
static const Rectangle btnVoltarFas  = {540, 560, 200, 52};
static const Rectangle btnOkTroca    = {520, 565, 240, 52};
static const Rectangle btnNovoPausa  = {540, 365, 200, 52};
static const Rectangle btnMenuPausa  = {540, 430, 200, 52};
static const Rectangle btnContinuar  = {540, 300, 200, 52};
static const Rectangle btnFimNovo    = {430, 620, 200, 52};
static const Rectangle btnFimMenu    = {650, 620, 200, 52};
static const Rectangle btnSecreta    = {1070, 650, 190, 50};
static const Rectangle btnMenos      = {960, 568, 50, 38};
static const Rectangle btnMais       = {1018, 568, 50, 38};
static const Rectangle btnReset      = {1076, 568, 80, 38};
static const Rectangle btnOkNome     = {520, 440, 240, 52};
static const Rectangle caixaNome     = {440, 330, 400, 60};
static const Rectangle painelMini    = {300, 140, 680, 450};
static const Rectangle barraSinc     = {360, 360, 560, 50};
static const Rectangle btnSinc       = {540, 450, 200, 52};
static const Rectangle btnEngrenagem = {12, 660, 58, 48};
static const Rectangle painelConfig = {330, 150, 620, 540};
static const Rectangle btnModoTela   = {490, 270, 300, 52};
static const Rectangle btnFecharConfig = {490, 640, 300, 44};

static Rectangle btnFase(int i) { return (Rectangle){250.0f + i * 160.0f, 300, 140, 100}; }

/* abas do ranking: n = quantidade de abas visiveis (centralizadas) */
static Rectangle abaRect(int i, int n)
{
    float total = n * 130.0f - 10.0f;
    return (Rectangle){640.0f - total / 2.0f + i * 130.0f, 150, 120, 44};
}

/* A aba Secreta so aparece quando ja existe pontuacao registrada nela */
static int abasVisiveis(void)
{
    return rankingLista[NUM_RANK - 1] ? NUM_RANK : NUM_RANK - 1;
}

/* barras de volume do menu de pausa: 0 = musica, 1 = efeitos */
static Rectangle barraVol(int i) { return (Rectangle){480, 548.0f + i * 50.0f, 300, 14}; }

/* ---------- Audio (gerado por codigo) ---------- */

static void tocar(Sound s)
{
    if (!audioOk) return;
    SetSoundVolume(s, volSfx);
    PlaySound(s);
}

static float freqMidi(int m)
{
    return 440.0f * powf(2.0f, (m - 69) / 12.0f);
}

static Sound somDeBuffer(short *buf, int frames)
{
    Wave w;
    memset(&w, 0, sizeof(w));
    w.frameCount = (unsigned int)frames;
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = buf;
    Sound s = LoadSoundFromWave(w);
    free(buf);
    return s;
}

static float sn(double ph)
{
    return sinf((float)fmod(ph, 6.283185307179586));
}

static unsigned lcg(unsigned *s)
{
    *s = *s * 1664525u + 1013904223u;
    return (*s >> 16) & 0x7fff;
}

/* Soma uma nota suave no buffer. Se passar do fim, a cauda volta ao inicio (loop sem corte). */
static void nota(float *b, int frames, double t0, double dur, double f, float vol,
                 float atk, float rel, float dec, float h2, float h3, float hd, float det)
{
    const double tp = 6.283185307179586;
    int n = (int)((dur + rel) * SR);
    long s0 = (long)(t0 * SR);
    for (int i = 0; i < n; i++) {
        double t = (double)i / SR;
        float env = t < atk ? (float)(t / atk) : 1.0f;
        env *= expf((float)(-dec * t));
        if (t > dur) {
            float q = 1.0f - (float)((t - dur) / rel);
            if (q < 0.0f) q = 0.0f;
            env *= q;
        }
        double ph = tp * f * t;
        float hf = expf((float)(-hd * t));
        float w = sn(ph) + h2 * hf * sn(2.0 * ph) + h3 * hf * sn(3.0 * ph);
        if (det > 0.0f) w += 0.7f * sn(tp * f * (1.0 + det) * t);
        b[(s0 + i) % frames] += vol * env * w;
    }
}

/* Compoe uma trilha: pad + baixo + arpejo + melodia em sino, com eco e filtro suave.
   tipo: 0 = m7, 1 = maj7, 2 = dominante (7) */
static Sound compor(const int *raiz, const int *tipo, int bars, float bpm, int arpDiv,
                    int probMel, unsigned seed, float volMel)
{
    static const int iv[3][4] = {{0, 3, 7, 10}, {0, 4, 7, 11}, {0, 4, 7, 10}};
    static const int ord[6] = {0, 1, 2, 3, 2, 1};

    double beat = 60.0 / bpm;
    double bar = 4.0 * beat;
    int frames = (int)(bars * bar * SR);

    float *dry = (float *)calloc((size_t)frames, sizeof(float));
    float *wet = (float *)calloc((size_t)frames, sizeof(float));
    float *eco = (float *)calloc((size_t)frames, sizeof(float));
    short *out = (short *)malloc(sizeof(short) * (size_t)frames);

    unsigned s = seed;
    int idx = 2;

    for (int b = 0; b < bars; b++) {
        double t0 = b * bar;
        int r0 = raiz[b];
        const int *v = iv[tipo[b]];

        /* pad: colchao suave de acordes */
        for (int k = 0; k < 4; k++)
            nota(dry, frames, t0, bar, freqMidi(r0 + v[k]), 0.10f, 0.7f, 1.0f, 0.0f, 0.15f, 0.0f, 0.0f, 0.004f);

        /* baixo macio */
        nota(dry, frames, t0, 2.6 * beat, freqMidi(r0 - 12), 0.22f, 0.05f, 0.5f, 0.35f, 0.20f, 0.0f, 0.0f, 0.0f);
        nota(dry, frames, t0 + 2.0 * beat, 1.6 * beat, freqMidi(r0 - 12), 0.14f, 0.05f, 0.5f, 0.35f, 0.20f, 0.0f, 0.0f, 0.0f);

        /* arpejo delicado */
        int steps = 4 * arpDiv;
        double step = beat / arpDiv;
        for (int i = 0; i < steps; i++) {
            int m = r0 + v[ord[i % 6]] + 12;
            nota(wet, frames, t0 + i * step, 0.2, freqMidi(m), 0.07f, 0.01f, 1.2f, 3.0f, 0.30f, 0.10f, 4.0f, 0.0f);
        }

        /* melodia em sino, sempre dentro do acorde */
        int pool[6] = {r0 + v[0] + 12, r0 + v[1] + 12, r0 + v[2] + 12, r0 + v[3] + 12,
                       r0 + v[0] + 24, r0 + v[1] + 24};
        for (int slot = 0; slot < 8; slot++) {
            if (b % 4 == 3 && slot >= 4) continue;                 /* respiro no fim da frase */
            if ((int)(lcg(&s) % 100) >= probMel) continue;
            idx += (int)(lcg(&s) % 5) - 2;
            if (idx < 0) idx = 0;
            if (idx > 5) idx = 5;
            nota(wet, frames, t0 + slot * 0.5 * beat, 0.3, freqMidi(pool[idx]), volMel,
                 0.015f, 2.5f, 1.4f, 0.35f, 0.12f, 3.0f, 0.0f);
        }
    }

    /* eco (delay pontuado), circular para o loop nao ter emenda */
    int D = (int)(0.75 * beat * SR);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < frames; i++) {
            int k = (i - D + frames) % frames;
            eco[i] = wet[i] + 0.45f * eco[k];
        }
    }

    /* mistura + filtro passa-baixa suave */
    const float a = 0.6f;
    float y = 0.0f;
    int ini = frames > 2000 ? frames - 2000 : 0;
    for (int i = ini; i < frames; i++) y += a * ((dry[i] + eco[i]) - y);

    float pico = 0.0001f;
    for (int i = 0; i < frames; i++) {
        y += a * ((dry[i] + eco[i]) - y);
        dry[i] = y;
        if (fabsf(y) > pico) pico = fabsf(y);
    }
    float g = 0.70f / pico;
    for (int i = 0; i < frames; i++) out[i] = (short)(dry[i] * g * 32000.0f);

    free(dry);
    free(wet);
    free(eco);
    return somDeBuffer(out, frames);
}

/* forma: 0 = senoide, 1 = quadrada. pulsos > 1 corta o som em bips. */
static Sound gerarEfeito(float f0, float f1, float dur, int forma, float vol, int pulsos)
{
    int frames = (int)(dur * SR);
    short *buf = (short *)malloc(sizeof(short) * (size_t)frames);
    float ph = 0.0f;
    for (int i = 0; i < frames; i++) {
        float u = (float)i / (float)frames;
        float f = f0 + (f1 - f0) * u;
        ph += 2.0f * 3.14159265f * f / SR;
        float s = sinf(ph);
        if (forma == 1) s = (s > 0 ? 1.0f : -1.0f) * 0.6f;
        float env = u < 0.03f ? u / 0.03f : (1.0f - u);
        if (env > 1.0f) env = 1.0f;
        float gate = 1.0f;
        if (pulsos > 1) {
            float g = fmodf(u * pulsos, 1.0f);
            gate = g < 0.65f ? 1.0f : 0.0f;
        }
        buf[i] = (short)(s * env * gate * vol * 30000.0f);
    }
    return somDeBuffer(buf, frames);
}

static Sound carregarOuGerar(const char *arq, Sound gerado)
{
    if (FileExists(arq)) {
        Sound s = LoadSound(arq);
        if (s.frameCount > 0) { UnloadSound(gerado); return s; }
    }
    return gerado;
}

static void iniciarAudio(void)
{
    InitAudioDevice();
    audioOk = IsAudioDeviceReady();
    if (!audioOk) return;

    sndClique = gerarEfeito(700.0f, 950.0f, 0.07f, 0, 0.5f, 1);
    sndBlack  = gerarEfeito(950.0f, 450.0f, 0.90f, 1, 0.35f, 2);
    sndSobre  = gerarEfeito(1500.0f, 1500.0f, 0.70f, 1, 0.30f, 6);
    sndEvento = gerarEfeito(600.0f, 950.0f, 0.40f, 0, 0.5f, 1);
    sndTick   = gerarEfeito(1100.0f, 1100.0f, 0.08f, 0, 0.6f, 1);
    sndOk     = gerarEfeito(500.0f, 1100.0f, 0.55f, 0, 0.5f, 1);
    sndFalha  = gerarEfeito(320.0f, 90.0f, 0.70f, 1, 0.35f, 1);
    sndApoio  = gerarEfeito(500.0f, 1200.0f, 0.25f, 0, 0.45f, 1);

    /* As musicas sao geradas sob demanda (ver garantirMusica) */
    for (int i = 0; i < NUM_MUS; i++) musPronta[i] = false;
    musAtual = -1;
}

static void encerrarAudio(void)
{
    if (!audioOk) return;
    UnloadSound(sndClique); UnloadSound(sndBlack); UnloadSound(sndSobre); UnloadSound(sndEvento);
    UnloadSound(sndTick); UnloadSound(sndOk); UnloadSound(sndFalha); UnloadSound(sndApoio);
    for (int i = 0; i < NUM_MUS; i++)
        if (musPronta[i]) UnloadSound(mus[i]);
    CloseAudioDevice();
}

/* ---------- Utilidades ---------- */

static float projX(float lon) { return 285.0f + (lon + 74.0f) * 15.8f; }
static float projY(float lat) { return 90.0f + (5.5f - lat) * 15.8f; }
/* Item 3: Curva de velocidade da rede suavizada para o nivel 4 e 5 */
static float velFase(void)       { return secreta ? 0.5f : 0.75f + 0.15f * (fase - 1); }
static float intensidade(void)   { return secreta ? 0.8f : 1.0f + 0.08f * (fase - 1); }
static int regioesNaFase(int f)  { return f <= 1 ? 3 : (f == 2 ? 4 : 5); }
static int maxEventos(void)      { return secreta ? 2 : 1 + fase / 2; }
static int diasFase(void)        { return secreta ? SEC_DIAS : DIAS; }
static float diaDur(void)        { return secreta ? SEC_DIA_SEG : DIA_SEG; }
static float lim(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float dist(Vector2 a, Vector2 b) { return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

static Color misturar(Color a, Color b, float t)
{
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), 255};
}

static Vector2 mouseVirtual(void)
{
    Vector2 m = GetMousePosition();

    float escalaX = viewport.width / (float)W;
    float escalaY = viewport.height / (float)H;

    if (escalaX <= 0.0f || escalaY <= 0.0f)
        return (Vector2){-10000.0f, -10000.0f};

    return (Vector2){
        (m.x - viewport.x) / escalaX,
        (m.y - viewport.y) / escalaY
    };
}

static void criarPoli(Regiao *g, const float (*d)[2], int n)
{
    g->n = n;
    for (int i = 0; i < n; i++) g->pt[i] = (Vector2){projX(d[i][0]), projY(d[i][1])};
}

static void textoCentro(const char *s, int x, int y, int tam, Color c)
{
    DrawText(s, x - MeasureText(s, tam) / 2, y, tam, c);
}

/* ---------- Musica por fase (geracao sob demanda) ---------- */

/* Item 5: Centralizado o aviso de carregamento de musica no meio exato da tela */
static void telaCarregando(const char *t)
{
    // 1. Recalcula a viewport para garantir o alinhamento correto em tela cheia
    float escala = fminf(
        (float)GetScreenWidth() / (float)W,
        (float)GetScreenHeight() / (float)H
    );
    Rectangle vp;
    vp.width = W * escala;
    vp.height = H * escala;
    vp.x = (GetScreenWidth() - vp.width) * 0.5f;
    vp.y = (GetScreenHeight() - vp.height) * 0.5f;

    // 2. Desenha na textura virtual (1280x720)
    BeginTextureMode(telaVirtual);
    ClearBackground((Color){8, 18, 40, 255});
    textoCentro(t, W / 2, 330, 30, WHITE);
    textoCentro("So leva um instante...", W / 2, 375, 18, LIGHTGRAY);
    EndTextureMode();

    // 3. Projeta a textura virtual na janela real com os valores da viewport
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(
        telaVirtual.texture,
        (Rectangle){0, 0, (float)W, -(float)H},
        vp,
        (Vector2){0, 0},
        0.0f,
        WHITE
    );
    EndDrawing();
}

static void garantirMusica(int i)
{
    if (musPronta[i]) return;
    telaCarregando(i == 0 ? "Preparando a musica do menu..." : "Carregando a musica da fase...");

    const Trilha *tr = &trilhas[i];
    mus[i] = compor(tr->raiz, tr->tipo, 16, tr->bpm, tr->arp, tr->prob, tr->seed, tr->vol);

    char arq[64];
    if (i == 0) snprintf(arq, sizeof(arq), "assets/musica_menu.wav");
    else if (i == NUM_MUS - 1) snprintf(arq, sizeof(arq), "assets/musica_secreta.wav");
    else snprintf(arq, sizeof(arq), "assets/musica_fase%d.wav", i);
    mus[i] = carregarOuGerar(arq, mus[i]);
    musPronta[i] = true;
}

static void atualizarMusica(void)
{
    if (!audioOk) return;
    int quer = 0;
    switch (tela) {
        case TELA_MENU: case TELA_AJUDA: case TELA_FASES: case TELA_RANK:
        case TELA_NOME: case TELA_FIM:
            quer = 0; break;
        default:
            quer = secreta ? (NUM_MUS - 1) : fase;
            break;
    }
    if (quer < 0) quer = 0;
    if (quer >= NUM_MUS) quer = NUM_MUS - 1;

    garantirMusica(quer);

    if (quer != musAtual) {
        if (musAtual >= 0 && musPronta[musAtual]) StopSound(mus[musAtual]);
        musAtual = quer;
    }

    /* volume da musica escolhido pelo jogador (menu de pausa) */
    float v = volMus;
    if (quer != 0) {
        v = volMus * 0.9f;
        if (tela == TELA_PAUSA) v = volMus * 0.30f;
        else if (tela == TELA_MINI) v = volMus * 0.55f;
    }
    SetSoundVolume(mus[quer], v);
    if (!IsSoundPlaying(mus[quer])) PlaySound(mus[quer]);
}

/* Item 2: Aumentada a duração base das mensagens para dar tempo de leitura */
static void mensagem(const char *s, const char *titulo, float t, int cor)
{
    if (s) snprintf(msg, sizeof(msg), "%s", s);
    if (titulo) snprintf(tituloMsg, sizeof(tituloMsg), "%s", titulo);
    msgT = t + 2.5f; /* Tempo adicional para leitura confortável */
    corMsg = cor;
}

static void aviso(const char *s)
{
    snprintf(toast, sizeof(toast), "%s", s);
    toastT = 4.0f; /* Item 2: Mais tempo de duração para toasts */
}

/* ---------- Bonus (combo) ---------- */

static void resetarCombo(bool avisar)
{
    if (avisar && combo > 1) aviso("Bonus perdido! Voltou para x1.");
    combo = 1;
    comboT = 0;
}

/* Chamado a cada frame com o tempo em que a rede ficou perfeita */
static void avancarCombo(float dt)
{
    if (combo >= COMBO_MAX) { comboT = 0; return; }
    comboT += dt;
    if (comboT >= COMBO_SEG) {
        comboT = 0;
        combo++;
        aviso(TextFormat("Bonus x%d!", combo));
        tocar(sndApoio);
    }
}

static bool clicou(Rectangle rc)
{
    bool ok = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouseVirtual(), rc);
    if (ok) tocar(sndClique);
    return ok;
}

static bool hover(Rectangle rc)
{
    return CheckCollisionPointRec(mouseVirtual(), rc);
}

static void caixa(Rectangle rc, float rd, Color fill, Color borda)
{
    DrawRectangleRounded((Rectangle){rc.x - 2, rc.y - 2, rc.width + 4, rc.height + 4}, rd, 8, borda);
    DrawRectangleRounded(rc, rd, 8, fill);
}

static void desenharBotao(Rectangle rc, const char *t, Color base)
{
    bool h = hover(rc);
    DrawRectangleRounded((Rectangle){rc.x + 2, rc.y + 5, rc.width, rc.height}, 0.3f, 8, Fade(BLACK, 0.4f));
    caixa(rc, 0.3f, h ? ColorBrightness(base, 0.2f) : base, h ? WHITE : Fade(WHITE, 0.4f));
    int tx = (int)(rc.x + rc.width / 2) - MeasureText(t, 23) / 2;
    DrawText(t, tx, (int)(rc.y + rc.height / 2) - 12, 23, WHITE);
}

static void coracao(float x, float y, float s, Color c)
{
    DrawCircle((int)(x - s * 0.5f), (int)(y - s * 0.2f), s * 0.55f, c);
    DrawCircle((int)(x + s * 0.5f), (int)(y - s * 0.2f), s * 0.55f, c);
    DrawTriangle((Vector2){x - s * 1.03f, y}, (Vector2){x, y + s * 1.15f}, (Vector2){x + s * 1.03f, y}, c);
}

static bool dentro(Vector2 p, const Vector2 *v, int n)
{
    bool ok = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((v[i].y > p.y) != (v[j].y > p.y)) &&
            p.x < (v[j].x - v[i].x) * (p.y - v[i].y) / (v[j].y - v[i].y) + v[i].x) ok = !ok;
    }
    return ok;
}

static int regiaoSob(Vector2 m)
{
    for (int i = 0; i < N; i++)
        if (r[i].ativa && dentro(m, r[i].pt, r[i].n)) return i;
    return -1;
}

/* Na fase secreta, o estado sob o mouse vem da matriz de celulas recortadas */
static int estadoSob(Vector2 m)
{
    int cx = (int)(m.x / CEL), cy = (int)(m.y / CEL);
    if (cx < 0 || cy < 0 || cx >= GW || cy >= GH) return -1;
    return gridEst[cy][cx];
}

static void preencher(const Vector2 *v, int n, Color c)
{
    float miny = v[0].y, maxy = v[0].y;
    for (int i = 1; i < n; i++) {
        if (v[i].y < miny) miny = v[i].y;
        if (v[i].y > maxy) maxy = v[i].y;
    }
    for (int y = (int)miny; y <= (int)maxy; y++) {
        float xs[64];
        int k = 0;
        float fy = y + 0.5f;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            if (((v[i].y > fy) != (v[j].y > fy)) && k < 64)
                xs[k++] = v[j].x + (fy - v[j].y) * (v[i].x - v[j].x) / (v[i].y - v[j].y);
        }
        for (int a = 1; a < k; a++) {
            float t = xs[a];
            int b = a - 1;
            while (b >= 0 && xs[b] > t) { xs[b + 1] = xs[b]; b--; }
            xs[b + 1] = t;
        }
        for (int a = 0; a + 1 < k; a += 2)
            DrawRectangle((int)xs[a], y, (int)(xs[a + 1] - xs[a]) + 1, 1, c);
    }
}

static void icone(int f, float x, float y, float t, float e)
{
    int ix = (int)x, iy = (int)y;

    /* selo de fundo */
    DrawCircle(ix, iy, 21 * e, Fade(BLACK, 0.55f));
    DrawCircleLines(ix, iy, 21 * e, Fade(WHITE, 0.75f));

    switch (f) {
        case 0: { /* hidreletrica: barragem + agua */
            Color concreto = (Color){190, 200, 215, 255};
            Vector2 tl = {x - 8 * e, y - 11 * e}, tr = {x + 8 * e, y - 11 * e};
            Vector2 bl = {x - 14 * e, y + 6 * e}, br = {x + 14 * e, y + 6 * e};
            DrawTriangle(tl, bl, br, concreto);
            DrawTriangle(tl, br, tr, concreto);
            DrawRectangle((int)(x - 3 * e), (int)(y - 8 * e), (int)(6 * e), (int)(10 * e), (Color){60, 80, 110, 255});
            for (int i = 0; i < 3; i++) {
                float d = fmodf(t * 18.0f + i * 5.0f, 12.0f * e);
                DrawCircle(ix, (int)(y - 2 * e + d), (int)(1.6f * e + 0.5f), SKYBLUE);
            }
            for (int k = -14; k <= 12; k += 2) {
                float w = sinf(t * 4.0f + k * 0.6f) * 1.5f * e;
                DrawRectangle((int)(x + k * e), (int)(y + 8 * e + w), (int)(2 * e + 1), (int)(4 * e), (Color){50, 150, 245, 255});
            }
            break;
        }
        case 1: { /* eolica: torre afunilada + 3 pas */
            Vector2 a = {x - 2.5f * e, y + 15 * e}, b = {x + 2.5f * e, y + 15 * e}, c = {x, y - 3 * e};
            DrawTriangle(a, b, c, RAYWHITE);
            DrawRectangle((int)(x - 7 * e), (int)(y + 14 * e), (int)(14 * e), (int)(2 * e + 1), (Color){120, 140, 170, 255});
            float a0 = t * 140.0f;
            for (int i = 0; i < 3; i++) {
                float ang = (a0 + i * 120.0f) * DEG2RAD;
                Vector2 ponta = {x + cosf(ang) * 15 * e, y - 3 * e + sinf(ang) * 15 * e};
                DrawLineEx((Vector2){x, y - 3 * e}, ponta, 2.6f * e, (Color){235, 245, 255, 255});
            }
            DrawCircle(ix, (int)(y - 3 * e), (int)(3 * e), (Color){255, 215, 60, 255});
            break;
        }
        case 2: { /* solar: painel inclinado + sol */
            float sx = x - 8 * e, sy = y - 9 * e;
            for (int i = 0; i < 8; i++) {
                float ang = (t * 40.0f + i * 45.0f) * DEG2RAD;
                DrawLineEx((Vector2){sx + cosf(ang) * 5.5f * e, sy + sinf(ang) * 5.5f * e},
                           (Vector2){sx + cosf(ang) * 8.5f * e, sy + sinf(ang) * 8.5f * e}, 1.4f * e, YELLOW);
            }
            DrawCircle((int)sx, (int)sy, (int)(4.5f * e), (Color){255, 220, 60, 255});

            Color painel = (Color){25, 70, 160, 255};
            Vector2 tl = {x - 4 * e, y - 1 * e}, tr = {x + 14 * e, y - 1 * e};
            Vector2 bl = {x - 12 * e, y + 10 * e}, br = {x + 10 * e, y + 10 * e};
            DrawTriangle(tl, bl, br, painel);
            DrawTriangle(tl, br, tr, painel);
            Color grade = (Color){140, 200, 255, 255};
            DrawLineV(tl, tr, grade); DrawLineV(bl, br, grade);
            DrawLineV(tl, bl, grade); DrawLineV(tr, br, grade);
            DrawLineV((Vector2){x - 8 * e, y + 4.5f * e}, (Vector2){x + 12 * e, y + 4.5f * e}, grade);
            DrawLineV((Vector2){x + 0.3f * e, y - 1 * e}, (Vector2){x - 1.5f * e, y + 10 * e}, grade);
            DrawLineV((Vector2){x + 5 * e, y - 1 * e}, (Vector2){x + 4 * e, y + 10 * e}, grade);
            DrawLineEx((Vector2){x - 1 * e, y + 10 * e}, (Vector2){x - 1 * e, y + 15 * e}, 2 * e, (Color){150, 160, 180, 255});
            DrawRectangle((int)(x - 6 * e), (int)(y + 14 * e), (int)(10 * e), (int)(2 * e + 1), (Color){150, 160, 180, 255});
            break;
        }
        case 3: { /* nuclear: torre de resfriamento + vapor */
            for (int i = 0; i < 10; i++) {
                float u = (i - 4.5f) / 4.5f;
                float w = (6.0f + 4.5f * u * u) * e;
                DrawRectangle((int)(x - w), (int)(y - 8 * e + i * 2.4f * e), (int)(2 * w), (int)(2.6f * e + 1),
                              i % 2 ? (Color){205, 210, 220, 255} : (Color){185, 192, 205, 255});
            }
            for (int i = 0; i < 3; i++) {
                float p = fmodf(t * 0.7f + i * 0.33f, 1.0f);
                DrawCircle((int)(x + sinf(t * 2.0f + i) * 3 * e), (int)(y - 10 * e - p * 9 * e),
                           (int)((3.0f + p * 2.5f) * e), Fade(WHITE, 0.75f * (1.0f - p)));
            }
            DrawCircle(ix, (int)(y + 3 * e), (int)(2 * e + 0.5f), (Color){255, 215, 60, 255});
            DrawEllipseLines(ix, (int)(y + 3 * e), 5 * e, 2.2f * e, (Color){255, 215, 60, 255});
            break;
        }
        default: { /* biomassa: broto + toras + chama */
            Color verde = (Color){90, 215, 110, 255};
            DrawLineEx((Vector2){x, y + 4 * e}, (Vector2){x, y - 8 * e}, 2 * e, (Color){60, 160, 80, 255});
            DrawEllipse((int)(x - 6 * e), (int)(y - 7 * e), 6 * e, 3.2f * e, verde);
            DrawEllipse((int)(x + 6 * e), (int)(y - 11 * e), 6 * e, 3.2f * e, (Color){120, 235, 130, 255});
            float fl = 1.0f + 0.15f * sinf(t * 12.0f);
            DrawTriangle((Vector2){x - 3 * e, y + 5 * e}, (Vector2){x + 3 * e, y + 5 * e},
                         (Vector2){x, y - 2 * e * fl}, (Color){255, 140, 40, 255});
            DrawTriangle((Vector2){x - 1.6f * e, y + 5 * e}, (Vector2){x + 1.6f * e, y + 5 * e},
                         (Vector2){x, y + 0.5f * e}, (Color){255, 220, 80, 255});
            DrawRectangleRounded((Rectangle){x - 12 * e, y + 8 * e, 24 * e, 4.5f * e}, 0.5f, 4, (Color){130, 85, 50, 255});
            DrawRectangleRounded((Rectangle){x - 8 * e, y + 12 * e, 16 * e, 4 * e}, 0.5f, 4, (Color){105, 68, 40, 255});
            DrawCircle((int)(x - 12 * e), (int)(y + 10.2f * e), (int)(2.2f * e), (Color){190, 140, 90, 255});
            break;
        }
    }
}

static Color corBatVisual(float b)
{
    if (b < LIM_BAIXO) return (Color){255, 70, 70, 255};
    if (b < LIM_BAIXO + 8) return (Color){255, 190, 60, 255};
    if (b > 100.0f) return (Color){255, 70, 70, 255};
    if (b > LIM_ALTO) return (Color){255, 140, 60, 255};
    if (b > LIM_ALTO - 6) return (Color){255, 210, 80, 255};
    return (Color){70, 225, 140, 255};
}

/* Seta de evento. Os triangulos precisam estar em sentido anti-horario (senao o raylib nao desenha). */
static void setaEvento(float x, float y, bool sobe, float esc)
{
    Color c = sobe ? (Color){70, 235, 130, 255} : (Color){255, 80, 80, 255};
    float s = 9.0f * esc;
    DrawCircle((int)x, (int)y, s + 6, Fade(BLACK, 0.6f));
    DrawCircleLines((int)x, (int)y, s + 6, Fade(c, 0.9f));
    if (sobe) DrawTriangle((Vector2){x, y - s}, (Vector2){x - s, y + s * 0.7f}, (Vector2){x + s, y + s * 0.7f}, c);
    else      DrawTriangle((Vector2){x, y + s}, (Vector2){x + s, y - s * 0.7f}, (Vector2){x - s, y - s * 0.7f}, c);
}

/* Segundos ate estourar (blackout/sobrecarga) mantendo o saldo atual; -1 se nao ha risco */
static float tempoCritico(float bat, float rate)
{
    if (bat < LIM_BAIXO && rate < -0.001f) return bat / (-rate);
    if (bat > LIM_ALTO && rate > 0.001f) return (BAT_MAX - bat) / rate;
    return -1.0f;
}

static float rateReg(int i)
{
    Regiao *g = &r[i];
    return (g->ajuste * g->multProd - g->consBase * g->multCons) * 4.0f * velFase();
}

static float rateEst(int i)
{
    Estado *e = &est[i];
    return (e->ajuste * r[e->reg].multProd - e->cons * r[e->reg].multCons) * 4.0f * velFase();
}

/* Anel de contagem regressiva */
static void cronometro(float x, float y, float raio, float seg, bool black)
{
    float t = (float)GetTime();
    float frac = lim(seg / TEMPO_CRONO, 0.0f, 1.0f);
    Color c = black ? (Color){255, 70, 70, 255} : (Color){255, 150, 40, 255};
    if (seg < 5.0f && sinf(t * 14.0f) > 0.0f) c = WHITE;
    DrawCircle((int)x, (int)y, raio + 3, Fade(BLACK, 0.8f));
    DrawRing((Vector2){x, y}, raio - 4, raio, 0, 360, 48, Fade(WHITE, 0.2f));
    DrawRing((Vector2){x, y}, raio - 4, raio, -90.0f, -90.0f + 360.0f * frac, 48, c);
    const char *txt = TextFormat("%d", (int)ceilf(seg));
    int tam = (int)(raio * 0.95f);
    DrawText(txt, (int)x - MeasureText(txt, tam) / 2, (int)y - tam / 2, tam, WHITE);
}

/* ---------- Ranking por fase (listas encadeadas + arquivo) e segredo ---------- */

static int inserirRanking(int idx, const char *nome, int p)
{
    if (idx < 0 || idx >= NUM_RANK) return 0;
    No *novo = (No *)malloc(sizeof(No));
    if (!novo) return 0;
    snprintf(novo->nome, sizeof(novo->nome), "%s", nome);
    novo->pontos = p;
    novo->prox = NULL;

    No **pp = &rankingLista[idx];
    int pos = 1;
    while (*pp && (*pp)->pontos >= p) { pp = &(*pp)->prox; pos++; }
    novo->prox = *pp;
    *pp = novo;

    int cnt = 0;
    No *a = rankingLista[idx];
    while (a) {
        cnt++;
        if (cnt == MAX_RANK) {
            No *x = a->prox;
            a->prox = NULL;
            while (x) { No *t = x->prox; free(x); x = t; }
            break;
        }
        a = a->prox;
    }
    return pos <= MAX_RANK ? pos : 0;
}

static void liberarRanking(void)
{
    for (int i = 0; i < NUM_RANK; i++) {
        No *a = rankingLista[i];
        while (a) { No *t = a->prox; free(a); a = t; }
        rankingLista[i] = NULL;
    }
}

static void carregarRanking(void)
{
    FILE *f = fopen("ranking_fases.txt", "r");
    if (!f) return;
    char nome[16];
    int idx, p;
    while (fscanf(f, "%d %15s %d", &idx, nome, &p) == 3) inserirRanking(idx, nome, p);
    fclose(f);
}

static void salvarRanking(void)
{
    FILE *f = fopen("ranking_fases.txt", "w");
    if (!f) return;
    for (int i = 0; i < NUM_RANK; i++)
        for (No *a = rankingLista[i]; a; a = a->prox) fprintf(f, "%d %s %d\n", i, a->nome, a->pontos);
    fclose(f);
}

static void carregarSegredo(void)
{
    FILE *f = fopen("segredo.txt", "r");
    if (!f) return;
    segredoOk = true;
    fclose(f);
}

static void salvarSegredo(void)
{
    segredoOk = true;
    FILE *f = fopen("segredo.txt", "w");
    if (!f) return;
    fprintf(f, "1\n");
    fclose(f);
}

static void fimDePartida(bool venceu)
{
    vitoria = venceu;
    arrasta = -1;
    posRanking = 0;
    nomeLen = 0;
    nomeJog[0] = '\0';
    rankAtual = secreta ? (NUM_RANK - 1) : (fase - 1);
    tocar(venceu ? sndOk : sndFalha);
    tela = (pontos > 0) ? TELA_NOME : TELA_FIM;
}

/* ---------- Alertas sonoros ---------- */

static void avaliarAlerta(int i, float bat, float rate, float *minT)
{
    bool lo = bat < LIM_BAIXO, hi = bat > LIM_ALTO;
    bool agora = lo || hi;
    if (agora && !alertaPrev[i]) tocar(lo ? sndBlack : sndSobre);
    alertaPrev[i] = agora;
    float t = tempoCritico(bat, rate);
    if (t >= 0.0f && t < *minT) *minT = t;
}

static void somContagem(float minT, float dt)
{
    if (minT <= 10.0f) {
        tickT -= dt;
        if (tickT <= 0.0f) {
            tocar(sndTick);
            tickT = minT < 4.0f ? 0.3f : (minT < 7.0f ? 0.6f : 1.0f);
        }
    } else {
        tickT = 0.0f;
    }
}

/* ---------- Inicializacao ---------- */

static void setReg(int id, const char *nome, Color cor, int fonte, float cons, float lon, float lat)
{
    r[id].nome = nome;
    r[id].cor = cor;
    r[id].fonte = fonte;
    r[id].consBase = cons;
    r[id].c = (Vector2){projX(lon), projY(lat)};
}

static void iniciarRegioes(void)
{
    memset(r, 0, sizeof(r));
    /* fonte: 0 hidreletrica, 1 eolica, 2 solar, 3 nuclear, 4 biomassa */
    setReg(NORTE,    "Norte",        (Color){52, 160, 100, 255}, 0, 0.92f, -62.0f, -4.6f);
    setReg(NORDESTE, "Nordeste",     (Color){235, 180, 45, 255}, 1, 0.98f, -42.5f, -8.2f);
    setReg(CENTRO,   "Centro-Oeste", (Color){220, 125, 50, 255}, 2, 0.93f, -54.0f, -14.5f);
    setReg(SUDESTE,  "Sudeste",      (Color){160, 95, 175, 255}, 3, 1.03f, -45.0f, -19.2f);
    setReg(SUL,      "Sul",          (Color){70, 115, 240, 255}, 4, 0.95f, -52.0f, -27.0f);

    criarPoli(&r[NORTE],    geoNorte,    (int)(sizeof(geoNorte)    / sizeof(geoNorte[0])));
    criarPoli(&r[NORDESTE], geoNordeste, (int)(sizeof(geoNordeste) / sizeof(geoNordeste[0])));
    criarPoli(&r[CENTRO],   geoCentro,   (int)(sizeof(geoCentro)   / sizeof(geoCentro[0])));
    criarPoli(&r[SUDESTE],  geoSudeste,  (int)(sizeof(geoSudeste)  / sizeof(geoSudeste[0])));
    criarPoli(&r[SUL],      geoSul,      (int)(sizeof(geoSul)      / sizeof(geoSul[0])));

    for (int i = 0; i < N; i++) {
        r[i].bat = 70.0f;
        r[i].ajuste = 1.0f;
        r[i].multProd = 1.0f;
        r[i].multCons = 1.0f;
        r[i].ativa = false;
    }
}

/* Recorta cada regiao em estados: cada celula pertence ao estado (da mesma regiao) mais proximo */
static void construirGrade(void)
{
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) gridEst[y][x] = -1;

    for (int y = 60 / CEL; y < GH; y++) {
        for (int x = 270 / CEL; x < 960 / CEL && x < GW; x++) {
            Vector2 p = {x * CEL + CEL / 2.0f, y * CEL + CEL / 2.0f};
            int rg = -1;
            for (int i = 0; i < N; i++)
                if (dentro(p, r[i].pt, r[i].n)) { rg = i; break; }
            if (rg < 0) continue;
            int melhor = -1;
            float md = 1e9f;
            for (int e = 0; e < NE; e++) {
                if (est[e].reg != rg) continue;
                float d = dist(p, est[e].c);
                if (d < md) { md = d; melhor = e; }
            }
            gridEst[y][x] = (short)melhor;
        }
    }
}

static void voltarMenu(void)
{
    podeSecreta = false;
    secreta = false;
    secretaVenceu = false;
    iniciarRegioes();
    tela = TELA_MENU;
}

static void prepararFase(void)
{
    int qtd = regioesNaFase(fase);
    for (int i = 0; i < N; i++) r[i].ativa = false;
    for (int k = 0; k < qtd; k++) {
        int id = ordemFase[k];
        r[id].ativa = true;
        r[id].bat = 70.0f;
        r[id].ajuste = 1.0f;
        r[id].multProd = 1.0f;
        r[id].multCons = 1.0f;
    }
    if (!r[selecionada].ativa) selecionada = ordemFase[0];

    dia = 1;
    tempoDia = 0;
    nev = 0;
    spawnT = 16.0f - 1.0f * (fase - 1);
    cooldown = 0;
    fluxoT = 0;
    msgT = 0;
    toastT = 0;
    arrasta = -1;
    tickT = 0;
    memset(alertaPrev, 0, sizeof(alertaPrev));
}

static void novoJogoFase(int f)
{
    podeSecreta = false;
    secreta = false;
    secretaVenceu = false;
    iniciarRegioes();
    fase = f;
    vidas = 3;
    vidasMax = 3;
    pontos = 0;
    pontosAcum = 0;
    tempoTotal = 0;
    bonusFase = 0;
    introJogo = true;
    selecionada = SUDESTE;
    resetarCombo(false);
    prepararFase();
    tela = TELA_TROCA;
}

static void novoJogo(void)
{
    novoJogoFase(1);
}

static void iniciarSecreta(void)
{
    secreta = true;
    podeSecreta = false;
    fase = FASES;
    vidasMax = 5;
    vidas = 5;
    resetarCombo(false);
    for (int i = 0; i < N; i++) {
        r[i].ativa = true;
        r[i].multProd = 1.0f;
        r[i].multCons = 1.0f;
    }
    for (int i = 0; i < NE; i++) {
        est[i].nome = estDef[i].nome;
        est[i].sig = estDef[i].sig;
        est[i].reg = estDef[i].reg;
        est[i].c = (Vector2){projX(estDef[i].lon), projY(estDef[i].lat)};
        est[i].bat = 65.0f;
        est[i].ajuste = 1.0f;
        est[i].cons = r[estDef[i].reg].consBase + (((i * 5) % 7) - 3) * 0.008f;
    }
    construirGrade();
    selEst = 23;
    dia = 1;
    tempoDia = 0;
    nev = 0;
    spawnT = 14.0f;
    cooldown = 0;
    fluxoT = 0;
    msgT = 0;
    toastT = 0;
    arrasta = -1;
    tickT = 0;
    memset(alertaPrev, 0, sizeof(alertaPrev));
    introJogo = false;
    tela = TELA_TROCA;
}

static void avancarFase(void)
{
    bonusFase = 500 * fase + vidas * 200;
    pontos += bonusFase;
    fase++;
    if (fase > FASES) {
        fase = FASES;
        podeSecreta = (vidas >= vidasMax);   /* nenhuma vida perdida */
        fimDePartida(true);
        return;
    }
    introJogo = false;
    prepararFase();
    tela = TELA_TROCA;
}

/* ---------- Eventos ---------- */

static bool linhaEfeito(char *buf, int sz, int id, int i, float *net)
{
    float k = intensidade();
    float p = evDefs[id].prod[i] * k, c = evDefs[id].cons[i] * k;
    *net = p - c;
    if (fabsf(p) < 0.01f && fabsf(c) < 0.01f) return false;
    int n = snprintf(buf, sz, "%s:", abrev[i]);
    if (fabsf(p) >= 0.01f) n += snprintf(buf + n, sz - n, " prod %+d%%", (int)roundf(p * 100));
    if (fabsf(c) >= 0.01f) n += snprintf(buf + n, sz - n, " cons %+d%%", (int)roundf(c * 100));
    return true;
}

static void spawnEvento(void)
{
    int id = -1;
    for (int tent = 0; tent < 40 && id < 0; tent++) {
        int c = GetRandomValue(0, NUM_EV - 1);
        bool rep = false;
        for (int i = 0; i < nev; i++) if (evs[i].id == c) rep = true;
        if (rep) continue;
        float rel = 0.0f;
        for (int i = 0; i < N; i++) {
            if (!r[i].ativa) continue;
            rel += fabsf(evDefs[c].prod[i]) + fabsf(evDefs[c].cons[i]);
        }
        if (rel > 0.05f) id = c;
    }
    if (id < 0) { spawnT = 4.0f; return; }

    evs[nev].id = id;
    evs[nev].dur = 25.0f;
    evs[nev].t = 25.0f;
    nev++;
    tocar(sndEvento);

    char resumo[200] = "";
    int cnt = 0;
    for (int i = 0; i < N && cnt < 3; i++) {
        char lin[80];
        float net;
        if (!r[i].ativa || !linhaEfeito(lin, sizeof(lin), id, i, &net)) continue;
        if (cnt) strncat(resumo, "  |  ", sizeof(resumo) - strlen(resumo) - 1);
        strncat(resumo, lin, sizeof(resumo) - strlen(resumo) - 1);
        cnt++;
    }
    mensagem(resumo, evDefs[id].nome, 5.0f, 1);
    if (secreta) spawnT = 22.0f + GetRandomValue(0, 50) / 10.0f;
    else spawnT = 24.0f - 2.5f * (fase - 1) + GetRandomValue(0, 40) / 10.0f;
}

static void atualizarMultiplicadores(void)
{
    float intens = intensidade();
    for (int i = 0; i < N; i++) {
        float mp = 0.92f, mc = 0.91f;
        for (int e = 0; e < nev; e++) {
            mp += evDefs[evs[e].id].prod[i] * intens;
            mc += evDefs[evs[e].id].cons[i] * intens;
        }
        /* Item 3: Flutuação de consumo reduzida para balanceamento nos níveis avançados */
        mc *= 1.0f + 0.025f * sinf(tempoTotal * 0.35f + i * 1.9f);
        if (mp < 0.25f) mp = 0.25f;
        if (mc < 0.35f) mc = 0.35f;
        r[i].multProd = mp;
        r[i].multCons = mc;
    }
}

static void tickEventos(float dt)
{
    for (int e = 0; e < nev; ) {
        evs[e].t -= dt;
        if (evs[e].t <= 0) { evs[e] = evs[nev - 1]; nev--; }
        else e++;
    }
    spawnT -= dt;
    if (spawnT <= 0) {
        if (nev < maxEventos()) spawnEvento();
        else spawnT = 3.0f;
    }
    atualizarMultiplicadores();
}

/* ---------- Minigames ---------- */

static Vector2 noEsq(int i)      { return (Vector2){435, 300 + i * 65}; }
static Vector2 noDir(int i)      { return (Vector2){845, 300 + i * 65}; }
static Vector2 centroSlot(int s) { return (Vector2){440.0f + (s % 3) * 200.0f, 330.0f + (s / 3) * 120.0f}; }
static Rectangle discRect(int s) { Vector2 c = centroSlot(s); return (Rectangle){c.x - 60, c.y - 40, 120, 80}; }
static Rectangle turbRect(int i) { return (Rectangle){370.0f + i * 150.0f, 260, 110, 230}; }

static const char *nomeAlvo(int i) { return secreta ? est[i].nome : r[i].nome; }

static void novoAlvo(void)
{
    int n = GetRandomValue(0, 5);
    if (n == mini.alvo) n = (n + 1) % 6;
    mini.alvo = n;
    mini.alvoT = lim(1.4f - 0.12f * (fase - 1), 0.8f, 1.4f);
}

static void iniciarMinigame(int id, bool blackout)
{
    memset(&mini, 0, sizeof(mini));
    mini.reg = id;
    mini.blackout = blackout;
    mini.arrasta = -1;
    arrasta = -1;
    resetarCombo(true);
    tocar(blackout ? sndBlack : sndSobre);
    int variante = GetRandomValue(0, 2);
    float f = (float)(fase - 1);

    if (blackout) mini.tipo = variante == 0 ? MINI_FIOS : (variante == 1 ? MINI_DISJUNTOR : MINI_GERADOR);
    else          mini.tipo = variante == 0 ? MINI_TURBINAS : (variante == 1 ? MINI_SINCRONIA : MINI_ALVOS);

    switch (mini.tipo) {
        case MINI_FIOS:
            mini.tempoMax = 30.0f - 2.0f * f;
            for (int i = 0; i < 4; i++) { mini.ordem[i] = i; mini.destino[i] = -1; }
            for (int i = 3; i > 0; i--) {
                int k = GetRandomValue(0, i);
                int t = mini.ordem[i];
                mini.ordem[i] = mini.ordem[k];
                mini.ordem[k] = t;
            }
            break;
        case MINI_DISJUNTOR:
            mini.tempoMax = 22.0f - 1.5f * f;
            for (int i = 0; i < 6; i++) mini.slot[i] = i + 1;
            for (int i = 5; i > 0; i--) {
                int k = GetRandomValue(0, i);
                int t = mini.slot[i];
                mini.slot[i] = mini.slot[k];
                mini.slot[k] = t;
            }
            mini.proximo = 1;
            break;
        case MINI_GERADOR:
            mini.tempoMax = 16.0f;
            mini.carga = 10.0f;
            break;
        case MINI_TURBINAS:
            mini.tempoMax = 15.0f;
            for (int i = 0; i < 4; i++) {
                mini.calor[i] = 15.0f + GetRandomValue(0, 15);
                mini.taxa[i] = 7.0f + 1.5f * f + GetRandomValue(0, 40) / 10.0f;
            }
            break;
        case MINI_SINCRONIA:
            mini.tempoMax = 22.0f - 1.0f * f;
            mini.vel = 0.55f + 0.12f * f;
            mini.larg = 0.24f - 0.02f * f;
            mini.marcador = 0.0f;
            mini.dir = 1.0f;
            mini.zona = GetRandomValue(25, 75) / 100.0f;
            break;
        default: /* ALVOS */
            mini.tempoMax = 22.0f;
            mini.alvo = -1;
            novoAlvo();
            break;
    }
    mini.tempo = mini.tempoMax;
    tela = TELA_MINI;
}

static void finalizarMini(void)
{
    float *b = secreta ? &est[mini.reg].bat : &r[mini.reg].bat;
    *b = mini.blackout ? 55.0f : 75.0f;
    tickT = 0;
    memset(alertaPrev, 0, sizeof(alertaPrev));
    if (mini.resultado == 1) {
        pontos += 100 * fase;
        mensagem("A rede foi estabilizada.", "MINIGAME VENCIDO", 3.0f, 2);
    } else {
        vidas--;
        if (vidas <= 0) { secretaVenceu = false; fimDePartida(false); return; }
        mensagem("Voce perdeu uma vida.", "FALHA NA REDE", 3.0f, 3);
    }
    tela = TELA_JOGO;
}

static void terminarMini(int res)
{
    mini.resultado = res;
    mini.fimT = 1.3f;
    tocar(res == 1 ? sndOk : sndFalha);
}

static void atualizarMini(float dt)
{
    Minigame *g = &mini;
    Vector2 m = mouseVirtual();
    bool press = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    if (g->flash > 0) g->flash -= dt;
    if (g->resultado) {
        g->fimT -= dt;
        if (g->fimT <= 0) finalizarMini();
        return;
    }
    g->tempo -= dt;

    switch (g->tipo) {
        case MINI_FIOS: {
            if (press) {
                for (int i = 0; i < 4; i++)
                    if (g->destino[i] < 0 && dist(m, noEsq(i)) < 24) g->arrasta = i;
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && g->arrasta >= 0) {
                for (int j = 0; j < 4; j++) {
                    if (dist(m, noDir(j)) < 28) {
                        if (g->ordem[g->arrasta] == j) g->destino[g->arrasta] = j;
                        else g->flash = 0.4f;
                    }
                }
                g->arrasta = -1;
            }
            int ok = 0;
            for (int i = 0; i < 4; i++) if (g->destino[i] >= 0) ok++;
            if (ok == 4) terminarMini(1);
            break;
        }
        case MINI_DISJUNTOR:
            if (press) {
                for (int s = 0; s < 6; s++) {
                    if (!CheckCollisionPointRec(m, discRect(s)) || g->on[s]) continue;
                    if (g->slot[s] == g->proximo) {
                        g->on[s] = true;
                        g->proximo++;
                        tocar(sndClique);
                        if (g->proximo > 6) terminarMini(1);
                    } else {
                        g->flash = 0.5f;
                        g->proximo = 1;
                        for (int k = 0; k < 6; k++) g->on[k] = false;
                        g->tempo -= 1.5f;
                    }
                }
            }
            break;
        case MINI_GERADOR:
            if (IsKeyPressed(KEY_SPACE) || clicou(btnSinc)) g->carga += 8.0f;
            g->carga -= (8.0f + 1.5f * (fase - 1)) * dt;
            g->carga = lim(g->carga, 0.0f, 100.0f);
            if (g->carga >= 100.0f) terminarMini(1);
            break;
        case MINI_TURBINAS:
            for (int i = 0; i < 4; i++) {
                g->calor[i] += g->taxa[i] * dt;
                if (press && CheckCollisionPointRec(m, turbRect(i))) { g->calor[i] -= 38.0f; tocar(sndClique); }
                g->calor[i] = lim(g->calor[i], 0.0f, 100.0f);
                if (g->calor[i] >= 100.0f) { terminarMini(2); return; }
            }
            if (g->tempo <= 0) { terminarMini(1); return; }
            break;
        case MINI_SINCRONIA:
            g->marcador += g->dir * g->vel * dt;
            if (g->marcador > 1.0f) { g->marcador = 1.0f; g->dir = -1.0f; }
            if (g->marcador < 0.0f) { g->marcador = 0.0f; g->dir = 1.0f; }
            if (IsKeyPressed(KEY_SPACE) || clicou(btnSinc)) {
                if (fabsf(g->marcador - g->zona) < g->larg / 2) {
                    g->acertos++;
                    if (g->acertos >= 3) { terminarMini(1); return; }
                    g->zona = lim(GetRandomValue(15, 85) / 100.0f, g->larg / 2 + 0.02f, 1.0f - g->larg / 2 - 0.02f);
                } else {
                    g->flash = 0.4f;
                    g->tempo -= 2.0f;
                }
            }
            break;
        default: /* ALVOS */
            g->alvoT -= dt;
            if (g->alvoT <= 0) {
                g->tempo -= 1.0f;
                g->flash = 0.3f;
                novoAlvo();
            }
            if (press) {
                for (int s = 0; s < 6; s++) {
                    if (!CheckCollisionPointRec(m, discRect(s))) continue;
                    if (s == g->alvo) {
                        g->acertos++;
                        tocar(sndClique);
                        if (g->acertos >= 8) { terminarMini(1); return; }
                        novoAlvo();
                    } else {
                        g->flash = 0.3f;
                        g->tempo -= 1.5f;
                    }
                    break;
                }
            }
            break;
    }
    if (g->tipo != MINI_TURBINAS && g->tempo <= 0 && !g->resultado) terminarMini(2);
}

/* ---------- Logica do jogo ---------- */

static void transferir(int o, int d)
{
    if (cooldown > 0) { aviso("Aguarde para enviar de novo."); return; }
    if (r[o].bat < 25.0f) { aviso("Origem sem energia suficiente."); return; }
    float ganho = vizinho[o][d] ? 8.0f : 5.0f;   /* matriz de vizinhanca */
    r[o].bat -= 10.0f;
    r[d].bat = lim(r[d].bat + ganho, 0.0f, BAT_MAX);
    cooldown = 1.0f;
    fluxoO = o;
    fluxoD = d;
    fluxoT = 1.2f;
    tocar(sndApoio);
}

static void transferirE(int o, int d)
{
    if (cooldown > 0) { aviso("Aguarde para enviar de novo."); return; }
    if (est[o].bat < 25.0f) { aviso("Origem sem energia suficiente."); return; }
    est[o].bat -= 10.0f;
    est[d].bat = lim(est[d].bat + 8.0f, 0.0f, BAT_MAX);
    cooldown = 0.5f;
    fluxoO = o;
    fluxoD = d;
    fluxoT = 1.0f;
    tocar(sndApoio);
}

static Rectangle linhaReg(int k) { return (Rectangle){955, 118.0f + k * 54.0f, 300, 48}; }

static void atualizarJogo(float dt)
{
    float vf = velFase();
    Vector2 m = mouseVirtual();

    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) { arrasta = -1; tela = TELA_PAUSA; return; }

    int qtd = regioesNaFase(fase);
    for (int k = 0; k < qtd; k++)
        if (clicou(linhaReg(k))) selecionada = ordemFase[k];

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int s = regiaoSob(m);
        if (s >= 0) { selecionada = s; arrasta = s; }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && arrasta >= 0) {
        int d = regiaoSob(m);
        if (d >= 0 && d != arrasta) transferir(arrasta, d);
        arrasta = -1;
    }

    Regiao *sel = &r[selecionada];
    float wheel = GetMouseWheelMove();
    if (wheel != 0) sel->ajuste += wheel * 0.05f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) sel->ajuste += 0.5f * dt;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) sel->ajuste -= 0.5f * dt;
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hover(btnMais)) sel->ajuste += 0.5f * dt;
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hover(btnMenos)) sel->ajuste -= 0.5f * dt;
    if (clicou(btnMais)) sel->ajuste += 0.03f;
    if (clicou(btnMenos)) sel->ajuste -= 0.03f;
    if (IsKeyPressed(KEY_R) || clicou(btnReset)) sel->ajuste = 1.0f;
    sel->ajuste = lim(sel->ajuste, 0.2f, 1.5f);

    tempoTotal += dt;
    tempoDia += dt;
    if (cooldown > 0) cooldown -= dt;
    if (msgT > 0) msgT -= dt;
    if (toastT > 0) toastT -= dt;
    if (fluxoT > 0) fluxoT -= dt;

    tickEventos(dt);

    bool tudoOk = true;
    float minT = 999.0f;
    for (int i = 0; i < N; i++) {
        Regiao *g = &r[i];
        if (!g->ativa) continue;
        float prod = g->ajuste * g->multProd;
        float cons = g->consBase * g->multCons;
        g->bat = lim(g->bat + (prod - cons) * 4.0f * vf * dt, 0.0f, BAT_MAX);

        if (g->bat < LIM_BAIXO || g->bat > LIM_ALTO) tudoOk = false;
        avaliarAlerta(i, g->bat, (prod - cons) * 4.0f * vf, &minT);
        if (g->bat <= 0.0f)     { iniciarMinigame(i, true);  return; }
        if (g->bat >= BAT_MAX)  { iniciarMinigame(i, false); return; }
    }
    somContagem(minT, dt);

    if (tudoOk) {
        avancarCombo(dt);
        /* Item 4: Reduzida a velocidade da pontuacao acumulada */
        pontosAcum += dt * 5.0f * fase * combo;
        int inteiro = (int)pontosAcum;
        pontos += inteiro;
        pontosAcum -= inteiro;
    }

    if (tempoDia >= DIA_SEG) {
        tempoDia = 0;
        dia++;
        if (dia > DIAS) { avancarFase(); return; }
        mensagem(TextFormat("Dia %d de %d", dia, DIAS), "NOVO DIA", 2.5f, 0);
    }
}

static void atualizarSecreta(float dt)
{
    Vector2 m = mouseVirtual();

    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) { arrasta = -1; tela = TELA_PAUSA; return; }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int s = estadoSob(m);
        if (s >= 0) { selEst = s; arrasta = s; }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && arrasta >= 0) {
        int d = estadoSob(m);
        if (d >= 0 && d != arrasta) transferirE(arrasta, d);
        arrasta = -1;
    }

    Estado *sel = &est[selEst];
    float wheel = GetMouseWheelMove();
    if (wheel != 0) sel->ajuste += wheel * 0.05f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) sel->ajuste += 0.5f * dt;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) sel->ajuste -= 0.5f * dt;
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hover(btnMais)) sel->ajuste += 0.5f * dt;
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hover(btnMenos)) sel->ajuste -= 0.5f * dt;
    if (clicou(btnMais)) sel->ajuste += 0.03f;
    if (clicou(btnMenos)) sel->ajuste -= 0.03f;
    if (IsKeyPressed(KEY_R) || clicou(btnReset)) sel->ajuste = 1.0f;
    sel->ajuste = lim(sel->ajuste, 0.2f, 1.5f);

    tempoTotal += dt;
    tempoDia += dt;
    if (cooldown > 0) cooldown -= dt;
    if (msgT > 0) msgT -= dt;
    if (toastT > 0) toastT -= dt;
    if (fluxoT > 0) fluxoT -= dt;

    tickEventos(dt);

    float vf = velFase();
    bool tudoOk = true;
    float minT = 999.0f;
    for (int i = 0; i < NE; i++) {
        Estado *e = &est[i];
        float prod = e->ajuste * r[e->reg].multProd;
        float cons = e->cons * r[e->reg].multCons;
        e->bat = lim(e->bat + (prod - cons) * 4.0f * vf * dt, 0.0f, BAT_MAX);
        if (e->bat < LIM_BAIXO || e->bat > LIM_ALTO) tudoOk = false;
        avaliarAlerta(i, e->bat, (prod - cons) * 4.0f * vf, &minT);
        if (e->bat <= 0.0f)    { iniciarMinigame(i, true);  return; }
        if (e->bat >= BAT_MAX) { iniciarMinigame(i, false); return; }
    }
    somContagem(minT, dt);

    if (tudoOk) {
        avancarCombo(dt);
        /* Item 4: Pontuacao reduzida na fase secreta tambem */
        pontosAcum += dt * 20.0f * combo;
        int inteiro = (int)pontosAcum;
        pontos += inteiro;
        pontosAcum -= inteiro;
    }

    if (tempoDia >= SEC_DIA_SEG) {
        tempoDia = 0;
        dia++;
        if (dia > SEC_DIAS) {
            pontos += 5000;
            secretaVenceu = true;
            salvarSegredo();
            fimDePartida(true);
            return;
        }
        mensagem(TextFormat("Dia %d de %d", dia, SEC_DIAS), "NOVO DIA", 2.5f, 0);
    }
}

/* ---------- Desenho: cena ---------- */

static void desenharMapa(void)
{
    float t = (float)GetTime();
    Vector2 m = mouseVirtual();
    int sob = regiaoSob(m);

    for (int i = 0; i < N; i++) {
        Regiao *g = &r[i];
        Color fill = (Color){36, 50, 72, 255};
        if (g->ativa) {
            fill = g->cor;
            if (i == selecionada) fill = ColorBrightness(fill, 0.2f);
            if (arrasta >= 0 && sob == i && i != arrasta) fill = ColorBrightness(fill, 0.3f);
        }
        preencher(g->pt, g->n, fill);
    }
    for (int i = 0; i < N; i++) {
        Regiao *g = &r[i];
        bool sel = g->ativa && i == selecionada;
        Color lc = g->ativa ? (Color){10, 18, 32, 255} : (Color){70, 85, 110, 255};
        if (sel) lc = WHITE;
        if (arrasta >= 0 && sob == i && i != arrasta) lc = (Color){255, 255, 140, 255};
        for (int k = 0; k < g->n; k++)
            DrawLineEx(g->pt[k], g->pt[(k + 1) % g->n], sel ? 3.0f : 2.0f, lc);
    }

    for (int i = 0; i < N; i++) {
        if (!r[i].ativa) continue;
        float net = 0;
        for (int e = 0; e < nev; e++)
            net += (evDefs[evs[e].id].prod[i] - evDefs[evs[e].id].cons[i]) * intensidade();
        if (fabsf(net) > 0.04f)
            setaEvento(r[i].c.x + 40, r[i].c.y - 24, net > 0, 1.0f + 0.15f * sinf(t * 6.0f));
    }

    for (int i = 0; i < N; i++) {
        Regiao *g = &r[i];
        if (!g->ativa) continue;
        float cx = g->c.x, cy = g->c.y;

        bool lo = g->bat < LIM_BAIXO, hi = g->bat > LIM_ALTO;
        if (lo || hi) {
            Color ac = lo ? (Color){255, 60, 60, 255} : (Color){255, 150, 40, 255};
            float pulso = 0.5f + 0.5f * sinf(t * 10.0f);
            DrawCircleLines((int)cx, (int)cy, 52 + pulso * 8, Fade(ac, 0.5f + 0.5f * pulso));
            DrawCircleLines((int)cx, (int)cy, 54 + pulso * 8, Fade(ac, 0.5f + 0.5f * pulso));
            const char *txt = lo ? "ALERTA: BLACKOUT" : "ALERTA: SOBRECARGA";
            int tw = MeasureText(txt, 14);
            DrawRectangle((int)cx - tw / 2 - 6, (int)cy + 36, tw + 12, 22, Fade(ac, 0.9f));
            DrawText(txt, (int)cx - tw / 2, (int)cy + 40, 14, WHITE);
        }

        icone(g->fonte, cx, cy - 24, t, 0.9f);
        textoCentro(g->nome, (int)cx, (int)cy - 2, 16, WHITE);

        Rectangle br = {cx - 45, cy + 16, 90, 14};
        DrawRectangleRounded((Rectangle){br.x - 2, br.y - 2, br.width + 4, br.height + 4}, 0.4f, 6, Fade(BLACK, 0.8f));
        DrawRectangleRounded(br, 0.4f, 6, (Color){30, 40, 55, 255});
        DrawRectangleRounded((Rectangle){br.x, br.y, fmaxf(4.0f, br.width * g->bat / BAT_MAX), br.height}, 0.4f, 6,
                             corBatVisual(g->bat));
        DrawRectangle((int)(br.x + br.width * LIM_BAIXO / BAT_MAX), (int)br.y - 2, 2, (int)br.height + 4, WHITE);
        DrawRectangle((int)(br.x + br.width * LIM_ALTO / BAT_MAX), (int)br.y - 2, 2, (int)br.height + 4, WHITE);
        textoCentro(TextFormat("%.0f%%", g->bat), (int)cx, (int)br.y + 1, 12, WHITE);

        /* cronometro ate o estouro */
        float seg = tempoCritico(g->bat, rateReg(i));
        if (seg >= 0.0f && seg <= TEMPO_CRONO) cronometro(cx - 74, cy + 6, 22, seg, lo);
    }

    if (arrasta >= 0 && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        DrawLineEx(r[arrasta].c, m, 5.0f, Fade(WHITE, 0.85f));
        DrawCircle((int)m.x, (int)m.y, 9, YELLOW);
        DrawCircleLines((int)m.x, (int)m.y, 13, WHITE);
    }
    if (fluxoT > 0) {
        float p = 1.0f - fluxoT / 1.2f;
        for (int k = 0; k < 6; k++) {
            float u = p * 1.3f - k * 0.07f;
            if (u <= 0 || u >= 1) continue;
            Vector2 a = r[fluxoO].c, b = r[fluxoD].c;
            DrawCircle((int)(a.x + (b.x - a.x) * u), (int)(a.y + (b.y - a.y) * u), 6, (Color){255, 230, 80, 255});
        }
    }
}

static void desenharMapaSecreto(void)
{
    float t = (float)GetTime();
    Vector2 m = mouseVirtual();
    int sob = estadoSob(m);
    float pulso = 0.5f + 0.5f * sinf(t * 10.0f);

    Color colEst[NE];
    for (int i = 0; i < NE; i++) {
        Color base = ColorBrightness(r[est[i].reg].cor, ((i * 7) % 5 - 2) * 0.07f);
        Color c = misturar(base, corBatVisual(est[i].bat), 0.30f);
        if (est[i].bat < LIM_BAIXO || est[i].bat > LIM_ALTO)
            c = misturar(c, corBatVisual(est[i].bat), 0.35f * pulso);
        if (i == selEst) c = ColorBrightness(c, 0.25f);
        if (i == sob) c = ColorBrightness(c, arrasta >= 0 ? 0.35f : 0.2f);
        colEst[i] = c;
    }

    int x0 = 270 / CEL, x1 = 960 / CEL;
    for (int y = 60 / CEL; y < GH; y++) {
        for (int x = x0; x < x1 && x < GW; x++) {
            int e = gridEst[y][x];
            if (e < 0) continue;
            DrawRectangle(x * CEL, y * CEL, CEL, CEL, colEst[e]);
        }
    }

    Color escuro = (Color){10, 18, 32, 255};
    for (int y = 60 / CEL; y < GH - 1; y++) {
        for (int x = x0; x < x1 && x < GW - 1; x++) {
            int a = gridEst[y][x];
            int dir = gridEst[y][x + 1], baixo = gridEst[y + 1][x];
            if (a != dir && !(a < 0 && dir < 0)) {
                bool s = (a == selEst || dir == selEst);
                if (a >= 0 && dir >= 0) DrawRectangle(x * CEL + CEL - 1, y * CEL, s ? 2 : 1, CEL, s ? WHITE : escuro);
                else if (s) DrawRectangle(x * CEL + CEL - 1, y * CEL, 2, CEL, WHITE);
            }
            if (a != baixo && !(a < 0 && baixo < 0)) {
                bool s = (a == selEst || baixo == selEst);
                if (a >= 0 && baixo >= 0) DrawRectangle(x * CEL, y * CEL + CEL - 1, CEL, s ? 2 : 1, s ? WHITE : escuro);
                else if (s) DrawRectangle(x * CEL, y * CEL + CEL - 1, CEL, 2, WHITE);
            }
        }
    }

    for (int i = 0; i < N; i++)
        for (int k = 0; k < r[i].n; k++)
            DrawLineEx(r[i].pt[k], r[i].pt[(k + 1) % r[i].n], 2.5f, escuro);

    if (arrasta >= 0 && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        DrawLineEx(est[arrasta].c, m, 4.0f, Fade(WHITE, 0.85f));
        DrawCircle((int)m.x, (int)m.y, 8, YELLOW);
    }
    if (fluxoT > 0) {
        float p = 1.0f - fluxoT / 1.0f;
        for (int k = 0; k < 5; k++) {
            float u = p * 1.3f - k * 0.07f;
            if (u <= 0 || u >= 1) continue;
            Vector2 a = est[fluxoO].c, b = est[fluxoD].c;
            DrawCircle((int)(a.x + (b.x - a.x) * u), (int)(a.y + (b.y - a.y) * u), 5, (Color){255, 230, 80, 255});
        }
    }

    for (int i = 0; i < NE; i++) {
        Estado *e = &est[i];
        bool lo = e->bat < LIM_BAIXO, hi = e->bat > LIM_ALTO;
        if (lo || hi) {
            Color ac = lo ? (Color){255, 60, 60, 255} : (Color){255, 150, 40, 255};
            DrawCircleLines((int)e->c.x, (int)e->c.y, 14 + pulso * 5, Fade(ac, 0.5f + 0.5f * pulso));
        }
        int tw = MeasureText(e->sig, 12);
        DrawText(e->sig, (int)e->c.x - tw / 2 + 1, (int)e->c.y - 6 + 1, 12, Fade(BLACK, 0.8f));
        DrawText(e->sig, (int)e->c.x - tw / 2, (int)e->c.y - 6, 12, WHITE);
        DrawRectangle((int)e->c.x - 12, (int)e->c.y + 8, 24, 5, Fade(BLACK, 0.75f));
        DrawRectangle((int)e->c.x - 11, (int)e->c.y + 9, (int)(22 * e->bat / BAT_MAX), 3, corBatVisual(e->bat));
    }

    /* cronometros (por cima de tudo) */
    for (int i = 0; i < NE; i++) {
        Estado *e = &est[i];
        float seg = tempoCritico(e->bat, rateEst(i));
        if (seg >= 0.0f && seg <= TEMPO_CRONO)
            cronometro(e->c.x, e->c.y - 26, 14, seg, e->bat < LIM_BAIXO);
    }

    for (int i = 0; i < N; i++) {
        float net = 0;
        for (int e = 0; e < nev; e++)
            net += (evDefs[evs[e].id].prod[i] - evDefs[evs[e].id].cons[i]) * intensidade();
        if (fabsf(net) > 0.04f)
            setaEvento(r[i].c.x, r[i].c.y - 34, net > 0, 1.0f + 0.15f * sinf(t * 6.0f));
    }

    if (sob >= 0) {
        Estado *e = &est[sob];
        const char *txt = TextFormat("%s (%s) %.0f%%", e->nome, abrev[e->reg], e->bat);
        int tw = MeasureText(txt, 16);
        float bx = lim(m.x + 14, 290, 940 - tw - 12), by = m.y - 34;
        DrawRectangle((int)bx, (int)by, tw + 12, 24, Fade(BLACK, 0.85f));
        DrawText(txt, (int)bx + 6, (int)by + 4, 16, WHITE);
    }
}

static void desenharEventos(void)
{
    float t = (float)GetTime();
    caixa((Rectangle){10, 80, 265, 540}, 0.03f, (Color){14, 26, 48, 240}, (Color){60, 90, 130, 255});
    DrawText("EVENTOS", 24, 92, 20, RAYWHITE);
    float y = 122;
    if (nev == 0) DrawText("Tudo calmo por enquanto.", 24, (int)y, 15, LIGHTGRAY);

    for (int e = 0; e < nev; e++) {
        int id = evs[e].id;
        char lin[N][80];
        float nets[N];
        int cnt = 0;
        for (int i = 0; i < N; i++)
            if (r[i].ativa && linhaEfeito(lin[cnt], 80, id, i, &nets[cnt])) cnt++;
        float h = 56.0f + cnt * 18.0f;
        caixa((Rectangle){20, y, 245, h}, 0.1f, (Color){28, 42, 70, 255}, (Color){240, 170, 50, 255});
        DrawText(evDefs[id].nome, 30, (int)y + 8, 18, (Color){255, 200, 80, 255});
        float f = lim(evs[e].t / evs[e].dur, 0.0f, 1.0f);
        DrawRectangle(30, (int)y + 32, 225, 6, (Color){20, 28, 45, 255});
        DrawRectangle(30, (int)y + 32, (int)(225 * f), 6, (Color){240, 170, 50, 255});
        for (int k = 0; k < cnt; k++)
            DrawText(lin[k], 30, (int)y + 44 + k * 18, 14, nets[k] > 0 ? (Color){110, 240, 150, 255} : (Color){255, 120, 120, 255});
        y += h + 8;
    }

    if (spawnT < 6.0f && nev < maxEventos() && sinf(t * 8.0f) > -0.3f)
        DrawText("EVENTO CHEGANDO!", 24, 595, 18, (Color){255, 200, 60, 255});
    if (toastT > 0) DrawText(toast, 14, 632, 15, (Color){255, 220, 120, 255});
}

static void desenharControleAjuste(float ajuste)
{
    DrawText("Producao (W/S, roda; R = reset):", 960, 540, 14, (Color){150, 170, 200, 255});
    desenharBotao(btnMenos, "-", (Color){170, 60, 60, 255});
    desenharBotao(btnMais, "+", (Color){50, 150, 90, 255});
    desenharBotao(btnReset, "Reset", (Color){70, 90, 150, 255});
    DrawText(TextFormat("%.0f%%", ajuste * 100), 1168, 574, 26, WHITE);
    DrawRectangle(960, 622, 290, 10, (Color){12, 20, 34, 255});
    DrawRectangle(960, 622, (int)(290 * ajuste / 1.5f), 10, (Color){80, 170, 255, 255});
}

static void desenharPainelDireito(void)
{
    caixa((Rectangle){945, 85, 320, 625}, 0.03f, (Color){14, 26, 48, 240}, (Color){60, 90, 130, 255});
    DrawText("REGIOES", 960, 92, 18, RAYWHITE);

    int qtd = regioesNaFase(fase);
    for (int k = 0; k < qtd; k++) {
        int id = ordemFase[k];
        Rectangle rc = linhaReg(k);
        bool sel = id == selecionada;
        caixa(rc, 0.2f, sel ? (Color){40, 62, 100, 255} : (Color){24, 36, 60, 255},
              sel ? WHITE : (hover(rc) ? (Color){120, 150, 200, 255} : (Color){50, 70, 100, 255}));
        DrawRectangle((int)rc.x + 8, (int)rc.y + 8, 8, 32, r[id].cor);
        DrawText(r[id].nome, (int)rc.x + 26, (int)rc.y + 6, 17, WHITE);
        Rectangle br = {rc.x + 26, rc.y + 28, 200, 10};
        DrawRectangleRounded(br, 0.5f, 6, (Color){12, 20, 34, 255});
        DrawRectangleRounded((Rectangle){br.x, br.y, fmaxf(4.0f, br.width * r[id].bat / BAT_MAX), br.height}, 0.5f, 6,
                             corBatVisual(r[id].bat));
        DrawRectangle((int)(br.x + br.width * LIM_BAIXO / BAT_MAX), (int)br.y - 2, 2, 14, WHITE);
        DrawRectangle((int)(br.x + br.width * LIM_ALTO / BAT_MAX), (int)br.y - 2, 2, 14, WHITE);
        DrawText(TextFormat("%.0f%%", r[id].bat), (int)rc.x + 238, (int)rc.y + 14, 20, corBatVisual(r[id].bat));
    }

    Regiao *g = &r[selecionada];
    float prod = g->ajuste * g->multProd, cons = g->consBase * g->multCons;
    float saldo = (prod - cons) * 4.0f * velFase();

    DrawText("REGIAO SELECIONADA", 960, 396, 14, (Color){150, 170, 200, 255});
    DrawText(g->nome, 960, 416, 26, g->cor);
    DrawText(TextFormat("Bateria: %.0f%%   (seguro 30-80)", g->bat), 960, 452, 17, WHITE);
    DrawText(TextFormat("Producao: %.0f%%   Consumo: %.0f%%", prod * 100, cons * 100), 960, 476, 16, LIGHTGRAY);
    DrawText(TextFormat("Saldo: %+.1f%%/s", saldo), 960, 500, 18,
             saldo > 0.05f ? (Color){255, 200, 80, 255} : (saldo < -0.05f ? (Color){255, 110, 110, 255} : (Color){110, 240, 150, 255}));
    desenharControleAjuste(g->ajuste);

    DrawText("Segure numa regiao e arraste ate outra", 960, 645, 14, LIGHTGRAY);
    DrawText("para apoiar (vizinha +8%, longe +5%).", 960, 663, 14, LIGHTGRAY);
    DrawRectangle(960, 688, 290, 8, (Color){12, 20, 34, 255});
    if (cooldown > 0) DrawRectangle(960, 688, (int)(290 * (1.0f - cooldown / 1.0f)), 8, (Color){255, 220, 80, 255});
    else DrawRectangle(960, 688, 290, 8, (Color){70, 225, 140, 255});
}

static void desenharPainelSecreto(void)
{
    caixa((Rectangle){945, 85, 320, 625}, 0.03f, (Color){14, 26, 48, 240}, (Color){255, 215, 60, 255});
    DrawText("FASE SECRETA", 960, 96, 22, (Color){255, 215, 60, 255});

    int alertas = 0;
    float media[N] = {0};
    int cnt[N] = {0};
    for (int i = 0; i < NE; i++) {
        if (est[i].bat < LIM_BAIXO || est[i].bat > LIM_ALTO) alertas++;
        media[est[i].reg] += est[i].bat;
        cnt[est[i].reg]++;
    }
    DrawText(TextFormat("Estados em alerta: %d / %d", alertas, NE), 960, 130, 18,
             alertas ? (Color){255, 150, 60, 255} : (Color){110, 240, 150, 255});

    DrawText("MEDIA POR REGIAO", 960, 168, 14, (Color){150, 170, 200, 255});
    for (int i = 0; i < N; i++) {
        float mm = cnt[i] ? media[i] / cnt[i] : 0;
        DrawRectangle(960, 192 + i * 28, 8, 20, r[i].cor);
        DrawText(r[i].nome, 976, 194 + i * 28, 16, WHITE);
        DrawRectangle(1090, 196 + i * 28, 120, 10, (Color){12, 20, 34, 255});
        DrawRectangle(1090, 196 + i * 28, (int)fmaxf(3.0f, 120 * mm / BAT_MAX), 10, corBatVisual(mm));
        DrawText(TextFormat("%.0f", mm), 1218, 192 + i * 28, 16, corBatVisual(mm));
    }

    Estado *g = &est[selEst];
    float prod = g->ajuste * r[g->reg].multProd, cons = g->cons * r[g->reg].multCons;
    float saldo = (prod - cons) * 4.0f * velFase();

    DrawText("ESTADO SELECIONADO", 960, 350, 14, (Color){150, 170, 200, 255});
    DrawText(g->nome, 960, 370, 26, r[g->reg].cor);
    DrawText(TextFormat("Regiao: %s", r[g->reg].nome), 960, 404, 16, LIGHTGRAY);
    DrawText(TextFormat("Bateria: %.0f%%   (seguro 30-80)", g->bat), 960, 430, 17, WHITE);
    DrawText(TextFormat("Producao: %.0f%%   Consumo: %.0f%%", prod * 100, cons * 100), 960, 456, 16, LIGHTGRAY);
    DrawText(TextFormat("Saldo: %+.1f%%/s", saldo), 960, 482, 18,
             saldo > 0.05f ? (Color){255, 200, 80, 255} : (saldo < -0.05f ? (Color){255, 110, 110, 255} : (Color){110, 240, 150, 255}));
    desenharControleAjuste(g->ajuste);

    DrawText("Clique num estado para seleciona-lo.", 960, 645, 14, LIGHTGRAY);
    DrawText("Arraste de um estado a outro: apoio.", 960, 663, 14, LIGHTGRAY);
    DrawRectangle(960, 688, 290, 8, (Color){12, 20, 34, 255});
    if (cooldown > 0) DrawRectangle(960, 688, (int)(290 * (1.0f - cooldown / 0.5f)), 8, (Color){255, 220, 80, 255});
    else DrawRectangle(960, 688, 290, 8, (Color){70, 225, 140, 255});
}

/* Indicador do bonus de pontos no topo da tela */
static void desenharCombo(void)
{
    Color cores[COMBO_MAX + 1] = {
        {150, 170, 200, 255}, {150, 170, 200, 255}, {110, 240, 150, 255},
        {255, 220, 70, 255}, {255, 150, 50, 255}, {255, 80, 110, 255}
    };
    Color c = cores[combo];
    Rectangle rc = {985, 8, 110, 34};
    float pulso = 0.5f + 0.5f * sinf((float)GetTime() * 8.0f);

    caixa(rc, 0.25f, (Color){20, 32, 54, 255}, combo > 1 ? Fade(c, combo == COMBO_MAX ? 0.6f + 0.4f * pulso : 1.0f) : (Color){60, 90, 130, 255});
    DrawText("BONUS", (int)rc.x + 8, (int)rc.y + 4, 12, (Color){150, 170, 200, 255});
    const char *tx = TextFormat("x%d", combo);
    DrawText(tx, (int)(rc.x + rc.width) - 10 - MeasureText(tx, 26), (int)rc.y + 3, 26, c);

    float f = combo >= COMBO_MAX ? 1.0f : lim(comboT / COMBO_SEG, 0.0f, 1.0f);
    DrawRectangle((int)rc.x + 8, (int)rc.y + 22, 50, 5, (Color){12, 20, 34, 255});
    DrawRectangle((int)rc.x + 8, (int)rc.y + 22, (int)(50 * f), 5, c);
}

static void desenharHUD(void)
{
    DrawRectangleGradientV(0, 0, W, 64, (Color){8, 16, 34, 255}, (Color){14, 30, 58, 255});
    DrawRectangle(0, 64, W, 2, (Color){50, 90, 140, 255});
    DrawText("VOLTERRA", 22, 16, 32, (Color){255, 215, 60, 255});
    if (secreta) textoCentro("FASE SECRETA", 420, 10, 24, (Color){255, 215, 60, 255});
    else textoCentro(TextFormat("FASE %d/%d", fase, FASES), 420, 10, 24, WHITE);
    textoCentro(TextFormat("Dia %d/%d", dia, diasFase()), 420, 38, 18, LIGHTGRAY);

    float prog = ((dia - 1) * diaDur() + tempoDia) / (diasFase() * diaDur());
    DrawRectangleRounded((Rectangle){520, 22, 250, 18}, 0.5f, 6, (Color){20, 32, 54, 255});
    DrawRectangleRounded((Rectangle){520, 22, fmaxf(6.0f, 250 * lim(prog, 0, 1)), 18}, 0.5f, 6, (Color){80, 170, 255, 255});
    for (int i = 1; i < diasFase(); i++) DrawRectangle(520 + 250 * i / diasFase(), 22, 2, 18, (Color){14, 26, 48, 255});
    textoCentro("progresso da fase", 645, 44, 12, (Color){150, 170, 200, 255});

    DrawText("VIDAS", 810, 8, 14, (Color){150, 170, 200, 255});
    for (int i = 0; i < vidasMax; i++) {
        float hx = 826.0f + i * 28.0f;
        coracao(hx, 32, 9, i < vidas ? (Color){240, 60, 80, 255} : (Color){50, 55, 70, 255});
        if (i < vidas) DrawCircle((int)hx - 7, 27, 2, Fade(WHITE, 0.7f));
    }

    desenharCombo();

    const char *p = TextFormat("%d", pontos);
    DrawText("PONTOS", 1260 - MeasureText("PONTOS", 14), 8, 14, (Color){150, 170, 200, 255});
    DrawText(p, 1260 - MeasureText(p, 30), 26, 30, WHITE);
    DrawText("[P] ou ESC: pausa e volume", 985, 46, 13, (Color){120, 140, 170, 255});
}

/* Item 2: Avisos reconfigurados para o centro da tela (y = 200) no estilo Pop-up */
static void desenharBanner(void)
{
    if (msgT <= 0) return;
    Color base[4] = {{40, 110, 200, 255}, {235, 150, 30, 255}, {40, 170, 100, 255}, {210, 50, 50, 255}};
    Color c = base[corMsg & 3];
    float a = msgT < 0.6f ? msgT / 0.6f : 1.0f;
    Rectangle b = {340, 200, 600, 90}; /* Posição centralizada estilo Pop-up */
    DrawRectangleRounded((Rectangle){b.x + 3, b.y + 6, b.width, b.height}, 0.2f, 8, Fade(BLACK, 0.6f * a));
    caixa(b, 0.2f, Fade(ColorBrightness(c, -0.55f), 0.98f * a), Fade(c, a));
    textoCentro(tituloMsg, W / 2, 212, 26, Fade(WHITE, a));
    int tam = 17;
    while (tam > 11 && MeasureText(msg, tam) > 570) tam--;
    textoCentro(msg, W / 2, 250, tam, Fade((Color){255, 240, 210, 255}, a));
    if (corMsg == 1 && msgT > 4.2f)
        DrawRectangleLinesEx((Rectangle){0, 0, W, H}, 8, Fade((Color){255, 170, 40, 255}, (msgT - 4.2f) / 0.8f));
}

static void cena(void)
{
    if (secreta) desenharMapaSecreto();
    else desenharMapa();
    desenharEventos();
    if (secreta) desenharPainelSecreto();
    else desenharPainelDireito();
    desenharHUD();
    desenharBanner();
}

/* ---------- Desenho: telas ---------- */

static const char *nomeAba(int i)
{
    static char buf[NUM_RANK][32];
    if (i == NUM_RANK - 1) snprintf(buf[i], sizeof(buf[i]), "Secreta");
    else snprintf(buf[i], sizeof(buf[i]), "Fase %d", i + 1);
    return buf[i];
}

static void desenharRankingLista(int cx, int y, int idx, int max)
{
    textoCentro(TextFormat("RANQUE - %s", nomeAba(idx)), cx, y, 20, (Color){255, 215, 60, 255});
    int i = 0;
    for (No *a = rankingLista[idx]; a && i < max; a = a->prox, i++) {
        Color c = (posRanking == i + 1 && tela == TELA_FIM) ? YELLOW : LIGHTGRAY;
        int ly = y + 30 + i * 24;
        DrawText(TextFormat("%d. %s", i + 1, a->nome), cx - 160, ly, 20, c);
        const char *pt = TextFormat("%d", a->pontos);
        DrawText(pt, cx + 160 - MeasureText(pt, 20), ly, 20, c);
    }
    if (i == 0) textoCentro("Ainda sem registros.", cx, y + 30, 18, GRAY);
}

static void desenharRank(void)
{
    int nAbas = abasVisiveis();
    if (abaRank >= nAbas) abaRank = 0;

    textoCentro("RANQUE", 640, 60, 60, (Color){255, 215, 60, 255});
    textoCentro("Melhores pontuacoes de cada fase", 640, 120, 18, LIGHTGRAY);

    for (int i = 0; i < nAbas; i++) {
        Rectangle rc = abaRect(i, nAbas);
        bool sel = (i == abaRank);
        bool sec = (i == NUM_RANK - 1);
        Color base = sec ? (Color){150, 110, 20, 255} : (Color){40, 100, 180, 255};
        Color fill = sel ? ColorBrightness(base, 0.15f) : ColorBrightness(base, -0.45f);
        caixa(rc, 0.25f, hover(rc) && !sel ? ColorBrightness(fill, 0.2f) : fill,
              sel ? (Color){255, 215, 60, 255} : Fade(WHITE, 0.3f));
        textoCentro(nomeAba(i), (int)(rc.x + rc.width / 2), (int)rc.y + 11, 22, sel ? WHITE : LIGHTGRAY);
    }

    caixa((Rectangle){340, 220, 600, 395}, 0.03f, (Color){14, 26, 48, 245}, (Color){60, 90, 130, 255});
    textoCentro(abaRank == NUM_RANK - 1 ? "FASE SECRETA" : TextFormat("FASE %d", abaRank + 1), 640, 232, 26,
                (Color){120, 200, 255, 255});
    DrawText("#", 370, 270, 16, (Color){150, 170, 200, 255});
    DrawText("Jogador", 420, 270, 16, (Color){150, 170, 200, 255});
    DrawText("Pontos", 880 - MeasureText("Pontos", 16), 270, 16, (Color){150, 170, 200, 255});
    DrawRectangle(360, 292, 560, 2, (Color){60, 90, 130, 255});

    int i = 0;
    for (No *a = rankingLista[abaRank]; a && i < MAX_RANK; a = a->prox, i++) {
        Color c = i == 0 ? (Color){255, 215, 60, 255} : (i == 1 ? (Color){210, 215, 225, 255}
                : (i == 2 ? (Color){210, 140, 80, 255} : RAYWHITE));
        int ly = 300 + i * 30;
        DrawText(TextFormat("%d", i + 1), 370, ly, 22, c);
        DrawText(a->nome, 420, ly, 22, c);
        const char *pt = TextFormat("%d", a->pontos);
        DrawText(pt, 880 - MeasureText(pt, 22), ly, 22, c);
    }
    if (i == 0) textoCentro("Ainda sem registros nesta fase.", 640, 400, 22, GRAY);

    desenharBotao(btnVoltar, "Menu", (Color){160, 55, 55, 255});
}

static void desenharMenu(void)
{
    float t = (float)GetTime();

    /* faiscas de energia subindo */
    for (int i = 0; i < 46; i++) {
        float seed = i * 37.1f;
        float x = fmodf(seed * 13.7f, (float)W);
        float vel = 18.0f + (i % 7) * 7.0f;
        float y = H + 20.0f - fmodf(t * vel + seed * 9.3f, H + 40.0f);
        float tam = 1.5f + (i % 3);
        float piscar = 0.35f + 0.35f * sinf(t * 3.0f + i);
        DrawCircle((int)(x + sinf(t + i) * 8.0f), (int)y, tam, Fade((Color){255, 220, 90, 255}, piscar));
    }

    /* titulo com brilho pulsante */
    float pulso = 0.5f + 0.5f * sinf(t * 2.0f);
    textoCentro("VOLTERRA", 643, 103, 92, Fade(BLACK, 0.6f));
    textoCentro("VOLTERRA", 640, 100, 92, Fade((Color){255, 150, 30, 255}, 0.25f + 0.25f * pulso));
    textoCentro("VOLTERRA", 640, 98, 92, (Color){255, 215, 60, 255});
    textoCentro("VOLTERRA", 640, 100, 92, (Color){255, 215, 60, 255});

    /* linha de luz animada */
    DrawRectangle(440, 198, 400, 3, Fade((Color){60, 90, 130, 255}, 0.8f));
    float hx = 440.0f + fmodf(t * 220.0f, 400.0f);
    DrawRectangleGradientH((int)fmaxf(440.0f, hx - 60), 198, 60, 3, Fade(YELLOW, 0.0f), YELLOW);

    textoCentro("Gerencie a energia do Brasil", 640, 210, 26, RAYWHITE);
    textoCentro("Evite blackouts. Evite sobrecargas. Mantenha o pais ligado.", 640, 242, 16, LIGHTGRAY);

    /* painel dos botoes */
    caixa((Rectangle){470, 268, 340, 322}, 0.06f, (Color){12, 22, 42, 235},
          Fade((Color){255, 215, 60, 255}, 0.55f + 0.2f * pulso));
    desenharBotao(btnNovo, "Novo Jogo", (Color){40, 140, 90, 255});
    desenharBotao(btnFases, "Escolher fase", (Color){120, 90, 170, 255});
    desenharBotao(btnRanque, "Ranque", (Color){190, 140, 30, 255});
    desenharBotao(btnComo, "Como jogar", (Color){40, 100, 180, 255});
    desenharBotao(btnSair, "Sair", (Color){160, 55, 55, 255});

    if (segredoOk) textoCentro("* Conquista secreta desbloqueada *", 640, 600, 15, (Color){255, 215, 60, 255});

    /* fontes de energia animadas */
    for (int i = 0; i < 5; i++) icone(i, 640.0f + (i - 2) * 76.0f, 650.0f, t, 1.0f);
    textoCentro("ENTER: novo jogo   |   P ou ESC: pausa e volume durante a partida", 640, 690, 14,
                (Color){120, 140, 170, 255});
}

static void desenharFases(void)
{
    textoCentro("ESCOLHER FASE", 640, 110, 56, (Color){255, 215, 60, 255});
    textoCentro("A partida comeca na fase escolhida, com 3 vidas e 0 pontos.", 640, 200, 20, LIGHTGRAY);
    for (int i = 0; i < FASES; i++) {
        Rectangle rc = btnFase(i);
        bool h = hover(rc);
        caixa(rc, 0.15f, h ? (Color){50, 80, 130, 255} : (Color){28, 44, 76, 255}, h ? WHITE : (Color){90, 120, 170, 255});
        textoCentro(TextFormat("%d", i + 1), (int)(rc.x + rc.width / 2), (int)rc.y + 12, 48, WHITE);
        textoCentro(TextFormat("%d regioes", regioesNaFase(i + 1)), (int)(rc.x + rc.width / 2), (int)rc.y + 70, 16, LIGHTGRAY);
    }
    desenharBotao(btnVoltarFas, "Voltar", (Color){160, 55, 55, 255});
}

static void desenharAjuda(void)
{
    textoCentro("COMO JOGAR", 640, 40, 44, (Color){255, 215, 60, 255});
    const char *tit[3] = {"1. Objetivo", "2. Controles", "3. Riscos e minigames"};
    const char *txt[3][7] = {
        {"Cada regiao tem uma bateria de 0% a 110%. Mantenha todas entre 30% e 80%.",
         "Acima de 80%: ALERTA de sobrecarga. Ao chegar em 110%, um minigame dispara.",
         "Abaixo de 30%: ALERTA de blackout. Ao chegar em 0%, um minigame dispara.",
         "Um cronometro no mapa mostra quantos segundos faltam para o estouro.",
         "Setas verdes no mapa indicam sobra de energia; vermelhas indicam falta.",
         "Sobreviva as 5 fases (4 dias de 30s). Cada fase comeca com uma pausa.",
         "Sem alertas, o bonus de pontos sobe de x1 ate x5. Qualquer alerta zera o bonus."},
        {"Clique numa regiao (ou na lista da direita) para seleciona-la.",
         "Ajuste a producao com W/S, roda do mouse ou botoes + e -. R (ou Reset) volta a 100%.",
         "Reduza a producao quando a bateria estiver alta; aumente quando estiver baixa.",
         "Para APOIAR outra regiao: segure o botao numa regiao e arraste ate a outra.",
         "O apoio tira 10% da origem: entrega 8% a vizinha ou 5% a uma regiao distante.",
         "Pausa: tecla P ou ESC. No menu de pausa voce ajusta o volume da musica e dos efeitos.",
         "Regioes novas liberam na fase 2 (4 regioes) e na fase 3 (5 regioes)."},
        {"Ao atingir 0% ou 110%, voce precisa vencer um minigame para estabilizar.",
         "Blackout: ligar os fios, religar disjuntores 1 a 6, ou girar o gerador (cliques).",
         "Sobrecarga: esfriar turbinas, parar o marcador na zona verde, ou desligar os alvos.",
         "Vencer estabiliza a regiao e da pontos. Falhar custa um coracao.",
         "Sem coracoes, o jogo acaba. Voce comeca com 3.",
         "No fim da partida, digite seu nome: a pontuacao entra no Ranque da fase.",
         "Dizem que quem nao perde nenhuma vida descobre algo especial..."}
    };
    caixa((Rectangle){190, 110, 900, 500}, 0.03f, (Color){14, 26, 48, 245}, (Color){60, 90, 130, 255});
    textoCentro(tit[paginaAjuda], 640, 135, 30, (Color){120, 200, 255, 255});
    for (int i = 0; i < 7; i++) DrawText(txt[paginaAjuda][i], 215, 200 + i * 50, 19, RAYWHITE);
    textoCentro(TextFormat("Pagina %d/3", paginaAjuda + 1), 640, 590, 16, LIGHTGRAY);
    if (paginaAjuda > 0) desenharBotao(btnAnt, "Anterior", (Color){60, 80, 130, 255});
    if (paginaAjuda < 2) desenharBotao(btnProx, "Proxima", (Color){60, 80, 130, 255});
    desenharBotao(btnVoltar, "Menu", (Color){160, 55, 55, 255});
}

/* Item 1: Tela de transição configurada para deixar super explícito o sistema de saldo e reset */
static void desenharTroca(void)
{
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.72f));

    if (secreta) {
        caixa((Rectangle){340, 90, 600, 510}, 0.04f,
              (Color){20, 20, 44, 255}, (Color){255, 215, 60, 255});

        textoCentro(TextFormat("FASE %d CONCLUIDA!", FASES),
                    640, 110, 28, (Color){110, 240, 150, 255});
        textoCentro("FASE SECRETA", 640, 150, 48, (Color){255, 215, 60, 255});
        textoCentro("Voce nao perdeu nenhuma vida!", 640, 210, 22, WHITE);

        DrawText("- Cada regiao agora se divide em seus 27 estados", 380, 260, 19, RAYWHITE);
        DrawText("- Gestao mais leve: rede mais lenta, eventos mais fracos", 380, 292, 19, RAYWHITE);
        DrawText("- 3 dias de 40 segundos e 5 vidas", 380, 324, 19, RAYWHITE);
        DrawText("- Clique num estado e ajuste a producao", 380, 356, 19, RAYWHITE);
        DrawText("- Arraste de um estado a outro para enviar energia", 380, 388, 19, RAYWHITE);
        DrawText("- Vencer rende +5000 pontos e a conquista secreta",
                 380, 420, 19, (Color){255, 215, 60, 255});

        textoCentro("O tempo so comeca quando voce clicar em Comecar.",
                    640, 510, 17, (Color){150, 190, 230, 255});
        desenharBotao(btnOkTroca, "Comecar", (Color){40, 140, 90, 255});
        return;
    }

    caixa((Rectangle){340, 75, 600, 540}, 0.04f,
          (Color){14, 26, 48, 255}, (Color){255, 215, 60, 255});

    int y = 95;

    if (!introJogo) {
        textoCentro(TextFormat("FASE %d CONCLUIDA!", fase - 1),
                    640, y, 30, (Color){110, 240, 150, 255});
        textoCentro(TextFormat("Bonus: +%d pontos", bonusFase),
                    640, y + 36, 20, YELLOW);
        y += 75;
    } else {
        textoCentro("PREPARE-SE", 640, y, 30, (Color){110, 240, 150, 255});
        y += 45;
    }

    textoCentro(TextFormat("FASE %d DE %d", fase, FASES), 640, y, 42, WHITE);
    y += 55;

    /* Destaque explicativo do Saldo e Reset no topo/início do jogo */
    caixa((Rectangle){360, (float)y, 560, 130}, 0.05f, (Color){24, 38, 68, 255}, (Color){70, 130, 200, 255});
    DrawText("COMO FUNCIONA O SALDO E RESET:", 375, y + 10, 18, (Color){255, 215, 60, 255});
    DrawText("- Saldo positivo (amarelo) CARREGA a bateria.", 375, y + 35, 15, RAYWHITE);
    DrawText("- Saldo negativo (vermelho) DESCARREGA a bateria.", 375, y + 55, 15, RAYWHITE);
    DrawText("- Use W/S, roda do mouse ou botoes +/- para equilibrar.", 375, y + 75, 15, RAYWHITE);
    DrawText("- Pressione R ou aperte RESET para voltar a 100%.", 375, y + 95, 15, (Color){110, 240, 150, 255});
    y += 145;

    int qtd = regioesNaFase(fase);

    DrawText(TextFormat("- Regioes na rede: %d", qtd), 380, y, 17, RAYWHITE);
    y += 24;

    if (fase > 1 && !introJogo && qtd > regioesNaFase(fase - 1)) {
        DrawText(TextFormat("- Nova regiao liberada: %s",
                            r[ordemFase[qtd - 1]].nome),
                 380, y, 17, (Color){255, 215, 60, 255});
        y += 24;
    }

    DrawText(TextFormat("- Eventos simultaneos: ate %d", maxEventos()),
             380, y, 17, RAYWHITE);
    y += 24;

    DrawText(TextFormat("- Velocidade da rede: %d%%",
                        (int)(velFase() * 100)),
             380, y, 17, RAYWHITE);
    y += 24;

    DrawText("- Mantenha as baterias na faixa segura (30%% a 80%%)", 380, y, 17, RAYWHITE);

    textoCentro("O tempo so comeca quando voce clicar em Comecar.",
                640, 525, 16, (Color){150, 190, 230, 255});
    desenharBotao(btnOkTroca, "Comecar", (Color){40, 140, 90, 255});
}

static void desenharMini(void)
{
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.75f));
    Color cor = mini.blackout ? (Color){255, 80, 80, 255} : (Color){255, 160, 50, 255};
    caixa(painelMini, 0.04f, (Color){16, 26, 46, 255}, cor);
    textoCentro(TextFormat("%s em %s", mini.blackout ? "BLACKOUT" : "SOBRECARGA", nomeAlvo(mini.reg)), 640, 158, 30, cor);

    const char *ins = "";
    switch (mini.tipo) {
        case MINI_FIOS:      ins = "Arraste cada fio da esquerda ate a ponta da mesma cor, a direita."; break;
        case MINI_DISJUNTOR: ins = "Clique nos disjuntores em ordem: 1, 2, 3, 4, 5, 6. Erro reinicia a sequencia."; break;
        case MINI_GERADOR:   ins = "Gire o gerador: clique no botao ou aperte ESPACO rapido ate encher 100%."; break;
        case MINI_TURBINAS:  ins = "Clique nas turbinas para esfria-las. Nao deixe nenhuma chegar a 100%!"; break;
        case MINI_SINCRONIA: ins = "Pare o marcador na zona verde (ESPACO ou botao). Acerte 3 vezes."; break;
        default:             ins = "Desligue o disjuntor ACESO antes que ele apague. Faca 8 acertos."; break;
    }
    textoCentro(ins, 640, 202, 18, LIGHTGRAY);

    float f = lim(mini.tempo / mini.tempoMax, 0.0f, 1.0f);
    DrawRectangleRounded((Rectangle){330, 545, 620, 14}, 0.5f, 6, (Color){12, 20, 34, 255});
    DrawRectangleRounded((Rectangle){330, 545, fmaxf(6.0f, 620 * f), 14}, 0.5f, 6,
                         f < 0.25f ? (Color){255, 80, 80, 255} : (Color){70, 225, 140, 255});
    textoCentro(TextFormat("%.1fs", fmaxf(mini.tempo, 0.0f)), 640, 564, 18, WHITE);

    Vector2 m = mouseVirtual();
    switch (mini.tipo) {
        case MINI_FIOS:
            DrawText("Origem", 400, 262, 18, LIGHTGRAY);
            DrawText("Destino", 810, 262, 18, LIGHTGRAY);
            for (int i = 0; i < 4; i++) {
                if (mini.destino[i] >= 0)
                    DrawLineEx(noEsq(i), noDir(mini.destino[i]), 7.0f, fios[mini.ordem[i]]);
            }
            if (mini.arrasta >= 0) DrawLineEx(noEsq(mini.arrasta), m, 7.0f, fios[mini.ordem[mini.arrasta]]);
            for (int i = 0; i < 4; i++) {
                DrawCircle((int)noEsq(i).x, (int)noEsq(i).y, 22, WHITE);
                DrawCircle((int)noEsq(i).x, (int)noEsq(i).y, 18, fios[mini.ordem[i]]);
                DrawCircle((int)noDir(i).x, (int)noDir(i).y, 22, WHITE);
                DrawCircle((int)noDir(i).x, (int)noDir(i).y, 18, fios[i]);
            }
            if (mini.flash > 0) DrawText("Cor errada!", 575, 510, 20, (Color){255, 100, 100, 255});
            break;
        case MINI_DISJUNTOR:
            for (int s = 0; s < 6; s++) {
                Rectangle rc = discRect(s);
                Color bg = mini.on[s] ? (Color){40, 170, 100, 255} : (Color){36, 50, 78, 255};
                Color bd = mini.flash > 0 ? (Color){255, 80, 80, 255} : (hover(rc) ? WHITE : (Color){90, 120, 170, 255});
                caixa(rc, 0.2f, bg, bd);
                textoCentro(TextFormat("%d", mini.slot[s]), (int)(rc.x + rc.width / 2), (int)rc.y + 12, 40, WHITE);
                textoCentro(mini.on[s] ? "LIGADO" : "desligado", (int)(rc.x + rc.width / 2), (int)rc.y + 58, 14,
                            mini.on[s] ? WHITE : LIGHTGRAY);
            }
            if (mini.proximo <= 6) textoCentro(TextFormat("Proximo: %d", mini.proximo), 640, 505, 20, YELLOW);
            break;
        case MINI_GERADOR:
            textoCentro(TextFormat("Carga: %.0f%%", mini.carga), 640, 300, 28, WHITE);
            caixa(barraSinc, 0.2f, (Color){12, 20, 34, 255}, (Color){90, 120, 170, 255});
            DrawRectangle((int)barraSinc.x + 4, (int)barraSinc.y + 4,
                          (int)((barraSinc.width - 8) * mini.carga / 100.0f), (int)barraSinc.height - 8,
                          mini.carga < 35 ? (Color){255, 90, 90, 255} : (Color){255, 215, 60, 255});
            desenharBotao(btnSinc, "GIRAR!", (Color){40, 100, 180, 255});
            break;
        case MINI_TURBINAS:
            for (int i = 0; i < 4; i++) {
                Rectangle rc = turbRect(i);
                float h = rc.height * mini.calor[i] / 100.0f;
                Color cc = mini.calor[i] < 50 ? (Color){70, 225, 140, 255}
                         : (mini.calor[i] < 80 ? (Color){255, 200, 60, 255} : (Color){255, 70, 70, 255});
                caixa(rc, 0.1f, (Color){12, 20, 34, 255}, mini.calor[i] > 80 ? (Color){255, 70, 70, 255} : (Color){90, 120, 170, 255});
                DrawRectangle((int)rc.x + 6, (int)(rc.y + rc.height - h), (int)rc.width - 12, (int)h, cc);
                textoCentro(TextFormat("%.0f%%", mini.calor[i]), (int)(rc.x + rc.width / 2), (int)rc.y + 100, 24, WHITE);
                textoCentro(TextFormat("Turbina %d", i + 1), (int)(rc.x + rc.width / 2), (int)(rc.y + rc.height) + 8, 16, LIGHTGRAY);
            }
            break;
        case MINI_SINCRONIA:
            textoCentro(TextFormat("Acertos: %d/3", mini.acertos), 640, 300, 26, WHITE);
            caixa(barraSinc, 0.2f, (Color){12, 20, 34, 255}, mini.flash > 0 ? (Color){255, 80, 80, 255} : (Color){90, 120, 170, 255});
            DrawRectangle((int)(barraSinc.x + (mini.zona - mini.larg / 2) * barraSinc.width), (int)barraSinc.y + 4,
                          (int)(mini.larg * barraSinc.width), (int)barraSinc.height - 8, (Color){70, 225, 140, 255});
            DrawRectangle((int)(barraSinc.x + mini.marcador * barraSinc.width) - 4, (int)barraSinc.y - 8, 8,
                          (int)barraSinc.height + 16, WHITE);
            desenharBotao(btnSinc, "PARAR", (Color){40, 100, 180, 255});
            break;
        default: /* ALVOS */
            textoCentro(TextFormat("Acertos: %d/8", mini.acertos), 640, 240, 24, WHITE);
            for (int s = 0; s < 6; s++) {
                Rectangle rc = discRect(s);
                bool aceso = s == mini.alvo;
                Color bg = aceso ? (Color){255, 150, 40, 255} : (Color){36, 50, 78, 255};
                Color bd = mini.flash > 0 ? (Color){255, 80, 80, 255} : (aceso ? WHITE : (Color){90, 120, 170, 255});
                caixa(rc, 0.2f, bg, bd);
                textoCentro(aceso ? "DESLIGUE!" : "ok", (int)(rc.x + rc.width / 2), (int)rc.y + 28, aceso ? 22 : 20,
                            aceso ? WHITE : LIGHTGRAY);
            }
            break;
    }

    if (mini.resultado) {
        DrawRectangle(300, 330, 680, 110, Fade(BLACK, 0.85f));
        textoCentro(mini.resultado == 1 ? "SUCESSO!" : "FALHOU!", 640, 355, 60,
                    mini.resultado == 1 ? (Color){110, 240, 150, 255} : (Color){255, 100, 100, 255});
    }
}

static void desenharNome(void)
{
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.85f));
    textoCentro(vitoria ? "FIM DE JOGO - VITORIA!" : "FIM DE JOGO", 640, 120, 54,
                vitoria ? (Color){110, 240, 150, 255} : (Color){255, 100, 100, 255});
    textoCentro(TextFormat("Pontuacao: %d", pontos), 640, 210, 36, YELLOW);
    textoCentro(TextFormat("Entra no Ranque: %s", nomeAba(rankAtual)), 640, 255, 18, (Color){150, 190, 230, 255});
    textoCentro("Digite seu nome para o ranking:", 640, 285, 22, WHITE);
    caixa(caixaNome, 0.15f, (Color){12, 20, 34, 255}, (Color){255, 215, 60, 255});
    DrawText(nomeJog, (int)caixaNome.x + 16, (int)caixaNome.y + 14, 34, WHITE);
    if (((int)(GetTime() * 2.0)) % 2 == 0)
        DrawRectangle((int)caixaNome.x + 18 + MeasureText(nomeJog, 34), (int)caixaNome.y + 12, 3, 36, WHITE);
    textoCentro(TextFormat("%d/%d letras  |  ENTER confirma", nomeLen, TAM_NOME), 640, 400, 16, LIGHTGRAY);
    desenharBotao(btnOkNome, "Confirmar", (Color){40, 140, 90, 255});
}

static void desenharFim(void)
{
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.8f));
    if (vitoria && secretaVenceu) {
        textoCentro("CONQUISTA SECRETA!", 640, 90, 70, (Color){255, 215, 60, 255});
        textoCentro("Voce dominou a rede dos 27 estados do Brasil!", 640, 180, 24, WHITE);
    } else if (vitoria) {
        textoCentro("VITORIA!", 640, 90, 80, (Color){110, 240, 150, 255});
        textoCentro("Voce manteve o Brasil ligado em todas as fases!", 640, 180, 24, WHITE);
    } else {
        textoCentro("BLACKOUT GERAL", 640, 90, 80, (Color){255, 90, 90, 255});
        textoCentro("A rede nao aguentou. Tente de novo!", 640, 180, 24, WHITE);
    }
    textoCentro(TextFormat("%s: %d pontos", nomeJog[0] ? nomeJog : "Jogador", pontos), 640, 235, 34, YELLOW);
    if (posRanking > 0) textoCentro(TextFormat("Novo recorde! Posicao %d no Ranque (%s)", posRanking, nomeAba(rankAtual)), 640, 290, 22, (Color){110, 240, 150, 255});
    desenharRankingLista(640, 350, rankAtual, 5);
    desenharBotao(btnFimNovo, "Novo Jogo", (Color){40, 140, 90, 255});
    desenharBotao(btnFimMenu, "Menu", (Color){40, 100, 180, 255});

    if (podeSecreta) {
        float p = 0.5f + 0.5f * sinf((float)GetTime() * 4.0f);
        DrawRectangleRoundedLines((Rectangle){btnSecreta.x - 6, btnSecreta.y - 6, btnSecreta.width + 12, btnSecreta.height + 12},
                                  0.3f, 8, Fade((Color){255, 215, 60, 255}, 0.4f + 0.6f * p));
        desenharBotao(btnSecreta, "Fase secreta", (Color){150, 110, 20, 255});
    }
}

static void desenharVolumes(void);
static void desenharEngrenagem(void);
static void desenharPainelConfiguracao(void);

static void desenharPainelConfiguracao(void)
{
    if (!painelConfigAberto) return;

    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.65f));

    caixa(painelConfig, 0.06f,
          (Color){14, 26, 48, 250},
          (Color){80, 130, 190, 255});

    textoCentro("CONFIGURACOES", 640, 170, 30,
                (Color){255, 215, 60, 255});

    desenharBotao(btnModoTela,
                  telaCheia ? "Modo: Tela cheia" : "Modo: Janela",
                  (Color){40, 100, 180, 255});

    desenharVolumes();

    desenharBotao(btnFecharConfig,
                  "Fechar",
                  (Color){160, 55, 55, 255});
}

/* Controles de volume do menu de pausa */
static void desenharVolumes(void)
{
    caixa((Rectangle){330, 498, 620, 130}, 0.08f, (Color){14, 26, 48, 245}, (Color){60, 90, 130, 255});
    textoCentro("VOLUME", 640, 506, 20, (Color){255, 215, 60, 255});

    const char *nomes[2] = {"Musica", "Efeitos"};
    float vals[2] = {volMus, volSfx};
    Color cores[2] = {(Color){80, 170, 255, 255}, (Color){255, 190, 60, 255}};

    for (int i = 0; i < 2; i++) {
        Rectangle b = barraVol(i);
        bool ativo = (arrastaVol == i);
        DrawText(nomes[i], 355, (int)b.y - 3, 22, WHITE);
        DrawRectangleRounded((Rectangle){b.x - 2, b.y - 2, b.width + 4, b.height + 4}, 0.5f, 6,
                             ativo ? WHITE : (Color){60, 90, 130, 255});
        DrawRectangleRounded(b, 0.5f, 6, (Color){12, 20, 34, 255});
        if (vals[i] > 0.01f)
            DrawRectangleRounded((Rectangle){b.x, b.y, b.width * vals[i], b.height}, 0.5f, 6, cores[i]);
        float kx = b.x + b.width * vals[i];
        DrawCircle((int)kx, (int)(b.y + b.height / 2), ativo ? 12 : 10, WHITE);
        DrawCircle((int)kx, (int)(b.y + b.height / 2), ativo ? 8 : 6, cores[i]);
        DrawText(TextFormat("%d%%", (int)roundf(vals[i] * 100.0f)), 810, (int)b.y - 3, 22, cores[i]);
    }
}

static void desenharEngrenagem(void)
{
    Rectangle rc = btnEngrenagem;
    bool h = hover(rc);

    DrawRectangleRounded(
        (Rectangle){rc.x + 2, rc.y + 4, rc.width, rc.height},
        0.25f, 8, Fade(BLACK, 0.45f)
    );

    DrawRectangleRounded(
        rc,
        0.25f, 8,
        h ? (Color){55, 75, 105, 255} : (Color){25, 38, 60, 245}
    );

    DrawRectangleRoundedLines(
        rc, 0.25f, 8,
        h ? WHITE : (Color){100, 130, 170, 255}
    );

    textoCentro(
        "CFG",
        (int)(rc.x + rc.width / 2),
        (int)(rc.y + 14),
        18,
        WHITE
    );
}

static void desenhar(void)
{
    ClearBackground(BLACK);
    DrawRectangleGradientV(0, 0, W, H,
                           (Color){8, 18, 40, 255},
                           (Color){14, 48, 78, 255});

    for (int x = 0; x < W; x += 64)
        DrawLine(x, 0, x, H, Fade(WHITE, 0.03f));

    for (int y = 0; y < H; y += 64)
        DrawLine(0, y, W, y, Fade(WHITE, 0.03f));

    switch (tela) {
        case TELA_MENU:
            desenharMapa();
            DrawRectangle(0, 0, W, H, Fade(BLACK, 0.55f));
            desenharMenu();
            break;

        case TELA_AJUDA:
            desenharAjuda();
            break;

        case TELA_FASES:
            desenharFases();
            break;

        case TELA_RANK:
            desenharRank();
            break;

        case TELA_JOGO:
            cena();
            break;

        case TELA_PAUSA:
            cena();
            DrawRectangle(0, 0, W, H, Fade(BLACK, 0.65f));
            textoCentro("PAUSADO", 640, 200, 60, WHITE);
            desenharBotao(btnContinuar, "Continuar", (Color){40, 140, 90, 255});
            desenharBotao(btnNovoPausa, "Novo Jogo", (Color){120, 90, 170, 255});
            desenharBotao(btnMenuPausa, "Menu", (Color){160, 55, 55, 255});
            desenharVolumes();
            break;

        case TELA_TROCA:
            cena();
            desenharTroca();
            break;

        case TELA_MINI:
            cena();
            desenharMini();
            break;

        case TELA_NOME:
            cena();
            desenharNome();
            break;

        case TELA_FIM:
            cena();
            desenharFim();
            break;
    }

    /* Desenha por cima da tela atual. */
    desenharEngrenagem();
    desenharPainelConfiguracao();
}

/* ---------- Atualizacao das telas ---------- */

static void atualizarNome(void)
{
    int c;
    while ((c = GetCharPressed()) > 0) {
        if (c >= 32 && c <= 125 && nomeLen < TAM_NOME) {
            nomeJog[nomeLen++] = (c == 32) ? '_' : (char)c;
            nomeJog[nomeLen] = '\0';
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && nomeLen > 0) nomeJog[--nomeLen] = '\0';

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || clicou(btnOkNome)) {
        if (nomeLen == 0) snprintf(nomeJog, sizeof(nomeJog), "Jogador");
        posRanking = inserirRanking(rankAtual, nomeJog, pontos);
        salvarRanking();
        tela = TELA_FIM;
    }
}

/* Barras de volume (pausa): clique ou arraste para ajustar */
static void atualizarVolumes(void)
{
    Vector2 m = mouseVirtual();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        for (int i = 0; i < 2; i++) {
            Rectangle b = barraVol(i);
            Rectangle z = {b.x - 14, b.y - 14, b.width + 28, b.height + 28};
            if (CheckCollisionPointRec(m, z)) arrastaVol = i;
        }
    }

    if (arrastaVol >= 0) {
        Rectangle b = barraVol(arrastaVol);
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float v = lim((m.x - b.x) / b.width, 0.0f, 1.0f);
            if (v < 0.02f) v = 0.0f;
            if (v > 0.98f) v = 1.0f;
            if (arrastaVol == 0) volMus = v;
            else volSfx = v;
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            if (arrastaVol == 1) tocar(sndClique);   /* teste do volume dos efeitos */
            arrastaVol = -1;
        }
    }
}

static void atualizar(float dt)
{
    if (clicou(btnEngrenagem)) {
        painelConfigAberto = !painelConfigAberto;
        arrastaVol = -1;
        return;
    }

    if (painelConfigAberto) {
        atualizarVolumes();

        if (arrastaVol >= 0) return;

        if (clicou(btnModoTela)) {
            ToggleFullscreen();
            telaCheia = !telaCheia;
            return;
        }

        if (clicou(btnFecharConfig) ||
            IsKeyPressed(KEY_ESCAPE) ||
            IsKeyPressed(KEY_ENTER)) {
            painelConfigAberto = false;
            return;
        }

        return; /* Não deixa clicar nos elementos que estão atrás do painel. */
    }
    
    switch (tela) {
        case TELA_MENU:
            if (clicou(btnNovo) || IsKeyPressed(KEY_ENTER)) novoJogo();
            else if (clicou(btnFases)) tela = TELA_FASES;
            else if (clicou(btnRanque)) { abaRank = 0; tela = TELA_RANK; }
            else if (clicou(btnComo)) { paginaAjuda = 0; tela = TELA_AJUDA; }
            else if (clicou(btnSair)) sair = true;
            break;
        case TELA_AJUDA:
            if (paginaAjuda > 0 && clicou(btnAnt)) paginaAjuda--;
            if (paginaAjuda < 2 && clicou(btnProx)) paginaAjuda++;
            if (clicou(btnVoltar) || IsKeyPressed(KEY_ESCAPE)) tela = TELA_MENU;
            break;
        case TELA_FASES:
            for (int i = 0; i < FASES; i++)
                if (clicou(btnFase(i))) { novoJogoFase(i + 1); return; }
            if (clicou(btnVoltarFas) || IsKeyPressed(KEY_ESCAPE)) tela = TELA_MENU;
            break;
        case TELA_RANK: {
            int nAbas = abasVisiveis();
            for (int i = 0; i < nAbas; i++)
                if (clicou(abaRect(i, nAbas))) abaRank = i;
            if (IsKeyPressed(KEY_RIGHT) && abaRank < nAbas - 1) abaRank++;
            if (IsKeyPressed(KEY_LEFT) && abaRank > 0) abaRank--;
            if (clicou(btnVoltar) || IsKeyPressed(KEY_ESCAPE)) tela = TELA_MENU;
            break;
        }
        case TELA_JOGO:
            if (secreta) atualizarSecreta(dt);
            else atualizarJogo(dt);
            break;
        case TELA_PAUSA:
            atualizarVolumes();
            if (arrastaVol >= 0) break;   /* enquanto arrasta a barra, ignora os botoes */
            if (clicou(btnContinuar) || IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) tela = TELA_JOGO;
            else if (clicou(btnNovoPausa)) novoJogo();
            else if (clicou(btnMenuPausa)) voltarMenu();
            break;
        case TELA_TROCA:
            if (clicou(btnOkTroca) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) tela = TELA_JOGO;
            break;
        case TELA_MINI:
            atualizarMini(dt);
            break;
        case TELA_NOME:
            atualizarNome();
            break;
        case TELA_FIM:
            if (podeSecreta && clicou(btnSecreta)) { iniciarSecreta(); return; }
            if (clicou(btnFimNovo)) novoJogo();
            else if (clicou(btnFimMenu) || IsKeyPressed(KEY_ENTER)) voltarMenu();
            break;
    }
}

int main(void)
{
    InitWindow(W, H, "VolTerra");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    SetRandomSeed((unsigned int)time(NULL));

    telaVirtual = LoadRenderTexture(W, H);
    SetTextureFilter(telaVirtual.texture, TEXTURE_FILTER_BILINEAR);

    iniciarAudio();
    iniciarRegioes();
    carregarRanking();
    carregarSegredo();

    while (!WindowShouldClose() && !sair) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        /*
         * Mantém a proporção 16:9 e centraliza a imagem.
         * Em 1920x1080, o jogo ocupa a tela inteira.
         */
        float escala = fminf(
            (float)GetScreenWidth() / (float)W,
            (float)GetScreenHeight() / (float)H
        );

        viewport.width = W * escala;
        viewport.height = H * escala;
        viewport.x = (GetScreenWidth() - viewport.width) * 0.5f;
        viewport.y = (GetScreenHeight() - viewport.height) * 0.5f;

        atualizar(dt);
        atualizarMusica();

        BeginTextureMode(telaVirtual);
        ClearBackground(BLACK);
        desenhar();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(
            telaVirtual.texture,
            (Rectangle){0, 0, (float)W, -(float)H},
            viewport,
            (Vector2){0, 0},
            0.0f,
            WHITE
        );

        EndDrawing();
    }

    liberarRanking();
    encerrarAudio();
    UnloadRenderTexture(telaVirtual);
    CloseWindow();
    return 0;
}