// src/main.cpp
// Simulação M/M/1 em C++ (evento discreto com lista de eventos)
// Autor: Erick da Silva Delvizio | Uso didático em aula
// Compilar: g++ -O2 -std=c++17 main.cpp -o mm1
// Executar: ./mm1 0.9 1.0 10000 12345
//           ./mm1 <lambda> <mu> <N_clientes_serv> [seed]

#include <iostream>  //Habilita entrada e saída padrão no console (cout, cin).
#include <queue>     //Disponibiliza filas FIFO e filas de prioridade para modelar filas e eventos.
#include <random>    //Permite geração de números aleatórios e distribuições estatísticas.
#include <iomanip>   //Controla a formatação da saída numérica (casas decimais, alinhamento).
#include <cmath>     //Fornece funções matemáticas e testes numéricos.
#include <stdexcept> //Define exceções padrão para tratamento de erros.
#include <string>    //Adicionado para corrigir erro com a função stoul
using namespace std; // Evita escrever o prefixo std:: antes dos nomes da biblioteca padrão.

enum class EventType
{
    ARRIVAL,
    DEPARTURE
}; // Define um tipo enumerado seguro para identificar eventos de chegada e saída na simulação.

// definição de um tipo composto struct Event com dois campos de dados—time (instante do evento) e type (o tipo do evento, vindo do
//  enum EventType)—e implementa um operador de comparação operator> que retorna true quando o evento atual ocorre depois de outro
//  (ou seja, quando time deste é maior que time do outro), permitindo ordenar eventos pelo tempo (por exemplo, em uma priority_queue).
//  Estrutura de evento (tempo e tipo) — usada na lista de eventos (FEL)
struct Event
{
    double time;
    EventType type;
    bool operator>(const Event &other) const { return time > other.time; }
};

// Comparador para tornar a priority_queue um min-heap por tempo
struct EventCmp
{
    bool operator()(const Event &a, const Event &b) const
    {
        return a.time > b.time;
    }
};

struct Simulation
{

    // =========================================================================
    // [Definir Listas e Variáveis] — PARÂMETROS DO MODELO (variáveis de controle)
    double lambda;     // taxa de chegadas
    double mu;         // taxa de serviço
    long N_target;     // número de clientes a completar (parada)
    unsigned int seed; // semente do RNG
    // =========================================================================

    // =========================================================================
    // [Definir Listas e Variáveis] — ESTADO DO SISTEMA
    double clock = 0.0;       // relógio de simulação
    bool server_busy = false; // *** "lista do servidor": indica ocupado/livre ***
    // Fila onde os clientes esperam:
    queue<double> q; // guarda tempos de chegada dos clientes em espera
    // Lista de eventos (FEL - Future Event List):
    priority_queue<Event, vector<Event>, EventCmp> fel;
    double current_customer_arrival_time = NAN; // chegada do cliente atualmente em serviço
    // =========================================================================

    // =========================================================================
    // [Definir Listas e Variáveis] — ACUMULADORES/ESTATÍSTICAS
    long completed = 0;
    double last_event_time = 0.0;
    double area_num_in_system = 0.0; // ∫ N(t) dt
    double area_queue_length = 0.0;  // ∫ Q(t) dt
    double busy_time = 0.0;          // ∫ 1{ocupado} dt
    double total_wait_time = 0.0;    // Σ Wq_i
    double total_system_time = 0.0;  // Σ W_i
    // =========================================================================

    // =========================================================================
    // [Configurar Streams] — GERADORES E DISTRIBUIÇÕES (chegada e serviço)
    mt19937 rng;                              // gerador base (Mersenne Twister)
    exponential_distribution<double> exp_arr; // interchegadas ~ Exp(lambda)
    exponential_distribution<double> exp_ser; // serviço       ~ Exp(mu)
    // =========================================================================

    // Construtor: injeta parâmetros e inicializa os streams (RNG + distribuições)
    Simulation(double l, double m, long N, unsigned int s = 12345)
        : lambda(l), mu(m), N_target(N), seed(s),
          rng(s), exp_arr(l), exp_ser(m)
    {
        if (lambda <= 0 || mu <= 0 || N_target <= 0)
            throw invalid_argument("Parametros invalidos: lambda, mu e N > 0.");
    }

    // Sorteios a partir dos streams configurados
    double exp_interarrival() { return exp_arr(rng); }
    double exp_service() { return exp_ser(rng); }

    inline int num_in_system() const
    {
        return static_cast<int>(q.size()) + (server_busy ? 1 : 0);
    }

    // =========================================================================
    // [Chamada de Inicialização] — prepara tudo para iniciar a simulação
    void init()
    {
        // ---------------------------------------------------------------------
        // [Definir Estados Iniciais] — zerar relógio, fila, agenda e estatísticas
        clock = 0.0;
        server_busy = false; // servidor começa LIVRE
        while (!q.empty())
            q.pop();           // fila começa VAZIA
        fel = decltype(fel)(); // limpa a lista de eventos (FEL)

        completed = 0;
        last_event_time = 0.0;
        area_num_in_system = 0.0;
        area_queue_length = 0.0;
        busy_time = 0.0;
        total_wait_time = 0.0;
        total_system_time = 0.0;
        current_customer_arrival_time = NAN;
        // ---------------------------------------------------------------------

        // ---------------------------------------------------------------------
        // [Agendar o Primeiro Evento] — chegada inicial: tempo_atual + interchegada
        schedule(Event{clock + exp_interarrival(), EventType::ARRIVAL});
        // ---------------------------------------------------------------------
    }
    // =========================================================================

    // Operação de agendamento (empurra evento para a FEL)
    void schedule(const Event &e) { fel.push(e); }

    // Atualiza integrais em função do tempo decorrido
    void update_time_avg_stats(double new_time)
    {
        double dt = new_time - last_event_time;
        if (dt < 0)
            dt = 0;
        area_num_in_system += num_in_system() * dt;
        area_queue_length += static_cast<int>(q.size()) * dt;
        if (server_busy)
            busy_time += dt;
        last_event_time = new_time;
    }

    // Lógica de chegada
    void process_arrival(const Event &e)
    {
        // Agenda a próxima chegada
        schedule(Event{e.time + exp_interarrival(), EventType::ARRIVAL});

        if (!server_busy)
        {
            server_busy = true; // servidor passa a ocupado
            current_customer_arrival_time = e.time;
            schedule(Event{e.time + exp_service(), EventType::DEPARTURE});
        }
        else
        {
            q.push(e.time); // cliente vai para a fila
        }
    }

    // Lógica de partida
    void process_departure(const Event &e)
    {
        completed++;
        if (!std::isnan(current_customer_arrival_time))
            total_system_time += (e.time - current_customer_arrival_time);

        if (q.empty())
        {
            server_busy = false; // servidor fica LIVRE
            current_customer_arrival_time = NAN;
        }
        else
        {
            double arrival_time_next = q.front();
            q.pop(); // próximo da FILA
            total_wait_time += (e.time - arrival_time_next);
            current_customer_arrival_time = arrival_time_next;
            schedule(Event{e.time + exp_service(), EventType::DEPARTURE});
        }
    }

    // Loop principal
    void run()
    {
        init(); // [Chamada de Inicialização]

        while (completed < N_target && !fel.empty())
        {
            Event e = fel.top();
            fel.pop(); // consome do topo da FEL
            update_time_avg_stats(e.time);
            clock = e.time;

            if (e.type == EventType::ARRIVAL)
                process_arrival(e);
            else
                process_departure(e);
        }
        update_time_avg_stats(clock);
        report();
    }

    // Relatório
    void report() const
    {
        double T = clock;
        double lambda_eff = (T > 0) ? (static_cast<double>(completed) / T) : NAN;
        double L = (T > 0) ? area_num_in_system / T : NAN;
        double Lq = (T > 0) ? area_queue_length / T : NAN;
        double rho = (T > 0) ? busy_time / T : NAN;
        double Wq = (completed > 0) ? total_wait_time / completed : NAN;
        double W = (completed > 0) ? total_system_time / completed : NAN;

        cout.setf(ios::fixed);
        cout << setprecision(6);
        cout << "==== RESULTADOS (M/M/1) ====\n";
        cout << "lambda=" << lambda << " mu=" << mu
             << "  N=" << completed << "  T=" << T << "\n";
        cout << "rho=" << rho << "  L=" << L << "  Lq=" << Lq
             << "  W=" << W << "  Wq=" << Wq << "\n";
    }
};

int main(int argc, char *argv[])
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 4 || argc > 5)
    {
        cerr << "Uso: " << argv[0] << " <lambda> <mu> <N_clientes_serv> [seed]\n";
        return 1;
    }
    double lambda = atof(argv[1]);
    double mu = atof(argv[2]);
    long N = atol(argv[3]);
    unsigned int seed = (argc == 5 ? static_cast<unsigned int>(stoul(argv[4])) : 12345u);

    try
    {
        Simulation sim(lambda, mu, N, seed);
        sim.run();
    }
    catch (const exception &ex)
    {
        cerr << "Erro: " << ex.what() << "\n";
        return 2;
    }
    return 0;
}
