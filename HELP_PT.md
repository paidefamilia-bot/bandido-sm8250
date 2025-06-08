# 📚 Kernel Bandido SM8250 - Ajuda & Documentação

## 🚀 Guia de Início Rápido

### **Instalação via TWRP (Recomendado)**
1. Baixe o ZIP do kernel mais recente em [Releases](https://github.com/paidefamilia-bot/bandido-sm8250/releases)
2. Inicialize no recovery TWRP
3. Faça o flash do arquivo ZIP do kernel
4. Reinicie o sistema

### **Instalação Manual**
```bash
# Flash da imagem do kernel
fastboot flash boot bandido-kernel.img

# Flash do dtbo (se incluído)
fastboot flash dtbo bandido-dtbo.img

# Reiniciar
fastboot reboot
```

---

## 🔧 Configuração do Sistema IA

### **Controles Básicos da IA**
```bash
# Habilitar/Desabilitar Scheduler IA
echo 1 > /sys/kernel/ai_scheduler/enabled  # Habilitar
echo 0 > /sys/kernel/ai_scheduler/enabled  # Desabilitar

# Definir Perfil de Performance
echo performance > /sys/kernel/ai_scheduler/profile    # Gaming/Alta Performance
echo balanced > /sys/kernel/ai_scheduler/profile       # Balanceado (Padrão)
echo power_save > /sys/kernel/ai_scheduler/profile     # Economia de Bateria
echo adaptive > /sys/kernel/ai_scheduler/profile       # IA Adaptativa
```

### **Configurações Avançadas da IA**
```bash
# Taxa de Aprendizado (0.0001 - 0.01)
echo 0.001 > /sys/kernel/ai_scheduler/learning_rate

# Janela de Predição (100-5000ms)
echo 1000 > /sys/kernel/ai_scheduler/prediction_window_ms

# Aceleração GPU
echo 1 > /sys/kernel/ai_scheduler/gpu_acceleration

# Camadas da Rede Neural (3-10)
echo 5 > /sys/kernel/ai_scheduler/nn_layers

# Iterações de Treinamento (100-10000)
echo 1000 > /sys/kernel/ai_scheduler/training_iterations
```

---

## 🎮 Otimização para Gaming

### **Configuração do Modo Gaming**
```bash
# Habilitar Perfil Gaming
echo performance > /sys/kernel/ai_scheduler/profile

# Boost da Frequência GPU
echo 1 > /sys/kernel/ai_scheduler/gpu_boost

# Desabilitar Economia de Energia
echo 0 > /sys/kernel/ai_scheduler/power_save_mode

# Definir Governor CPU para Performance
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
```

### **Otimizações Específicas para Jogos**
```bash
# PUBG Mobile / Call of Duty Mobile
echo gaming_fps > /sys/kernel/ai_scheduler/game_mode

# Genshin Impact / Jogos 3D Pesados
echo gaming_3d > /sys/kernel/ai_scheduler/game_mode

# Emuladores (PSP, PS2, etc.)
echo gaming_emulator > /sys/kernel/ai_scheduler/game_mode
```

---

## 🔋 Otimização de Bateria

### **Modo Economia de Bateria**
```bash
# Habilitar Perfil Economia de Energia
echo power_save > /sys/kernel/ai_scheduler/profile

# Habilitar Economia Agressiva de Energia
echo 1 > /sys/kernel/ai_scheduler/aggressive_power_save

# Reduzir Atividade em Background
echo 1 > /sys/kernel/ai_scheduler/background_limit

# Habilitar Throttling Térmico
echo 1 > /sys/kernel/ai_scheduler/thermal_throttle
```

### **Predição de Bateria**
```bash
# Ver Predição de Bateria
cat /proc/ai_scheduler/battery_prediction

# Padrões de Uso da Bateria
cat /proc/ai_scheduler/usage_patterns

# Otimização de Carregamento
echo 1 > /sys/kernel/ai_scheduler/smart_charging
```

---

## 🌡️ Gerenciamento Térmico

### **Controles Térmicos**
```bash
# Ver Status Térmico
cat /proc/ai_scheduler/thermal_status

# Definir Perfil Térmico
echo conservative > /sys/kernel/ai_scheduler/thermal_profile  # Conservador
echo balanced > /sys/kernel/ai_scheduler/thermal_profile      # Balanceado
echo aggressive > /sys/kernel/ai_scheduler/thermal_profile    # Agressivo

# Proteção Térmica de Emergência
echo 1 > /sys/kernel/ai_scheduler/emergency_thermal
```

### **Monitoramento de Temperatura**
```bash
# Temperatura da CPU
cat /sys/class/thermal/thermal_zone0/temp

# Temperatura da GPU
cat /sys/class/thermal/thermal_zone1/temp

# Temperatura da Bateria
cat /sys/class/power_supply/battery/temp
```

---

## 📊 Monitoramento de Performance

### **Estatísticas em Tempo Real**
```bash
# Status do Sistema IA
cat /proc/ai_scheduler/status

# Métricas de Performance
cat /proc/ai_scheduler/performance

# Estatísticas de Aprendizado
cat /proc/ai_scheduler/learning_stats

# Status da Aceleração GPU
cat /proc/ai_scheduler/gpu_stats
```

### **Monitoramento Contínuo**
```bash
# Observar Performance IA (atualiza a cada segundo)
watch -n 1 cat /proc/ai_scheduler/performance

# Monitorar Predições
watch -n 2 cat /proc/ai_scheduler/predictions

# Acompanhar Progresso do Aprendizado
watch -n 5 cat /proc/ai_scheduler/learning_progress
```

---

## 🛠️ Solução de Problemas

### **Problemas Comuns**

#### **Sistema IA Não Funcionando**
```bash
# Verificar se IA está habilitada
cat /sys/kernel/ai_scheduler/enabled

# Reiniciar sistema IA
echo 0 > /sys/kernel/ai_scheduler/enabled
echo 1 > /sys/kernel/ai_scheduler/enabled

# Resetar dados de aprendizado IA
echo 1 > /sys/kernel/ai_scheduler/reset_learning
```

#### **Performance Ruim em Jogos**
```bash
# Forçar modo performance
echo performance > /sys/kernel/ai_scheduler/profile

# Verificar aceleração GPU
cat /sys/kernel/ai_scheduler/gpu_acceleration

# Verificar throttling térmico
cat /proc/ai_scheduler/thermal_status
```

#### **Problemas de Drain de Bateria**
```bash
# Habilitar modo economia de energia
echo power_save > /sys/kernel/ai_scheduler/profile

# Verificar processos em background
cat /proc/ai_scheduler/background_activity

# Revisar consumo de energia
cat /proc/ai_scheduler/power_consumption
```

### **Resetar para Padrões**
```bash
# Resetar todas as configurações IA
echo 1 > /sys/kernel/ai_scheduler/reset_all

# Restaurar perfil padrão
echo balanced > /sys/kernel/ai_scheduler/profile

# Limpar dados de aprendizado
echo 1 > /sys/kernel/ai_scheduler/clear_learning_data
```

---

## 🔍 Recursos Avançados

### **Treinamento IA Personalizado**
```bash
# Iniciar sessão de treinamento personalizada
echo 1 > /sys/kernel/ai_scheduler/start_training

# Definir fonte de dados de treinamento
echo user_patterns > /sys/kernel/ai_scheduler/training_source

# Monitorar progresso do treinamento
cat /proc/ai_scheduler/training_progress
```

### **Reconhecimento de Padrões**
```bash
# Ver padrões detectados
cat /proc/ai_scheduler/detected_patterns

# Forçar aprendizado de padrões
echo 1 > /sys/kernel/ai_scheduler/force_pattern_learning

# Exportar padrões
cat /proc/ai_scheduler/export_patterns > /sdcard/ai_patterns.txt
```

### **Ajuste da Rede Neural**
```bash
# Ajustar arquitetura da rede
echo 7 > /sys/kernel/ai_scheduler/hidden_layers
echo 128 > /sys/kernel/ai_scheduler/neurons_per_layer

# Definir função de ativação
echo relu > /sys/kernel/ai_scheduler/activation_function

# Configurar otimizador
echo adam > /sys/kernel/ai_scheduler/optimizer
```

---

## 📱 Configurações Específicas por Dispositivo

### **OnePlus 8/8 Pro**
```bash
# Otimizar para OnePlus
echo oneplus > /sys/kernel/ai_scheduler/device_profile

# Habilitar recursos específicos OnePlus
echo 1 > /sys/kernel/ai_scheduler/oneplus_optimizations
```

### **Samsung Galaxy S20**
```bash
# Otimizar para Samsung
echo samsung > /sys/kernel/ai_scheduler/device_profile

# Habilitar recursos específicos Samsung
echo 1 > /sys/kernel/ai_scheduler/samsung_optimizations
```

### **Xiaomi Mi 10**
```bash
# Otimizar para Xiaomi
echo xiaomi > /sys/kernel/ai_scheduler/device_profile

# Habilitar otimizações MIUI
echo 1 > /sys/kernel/ai_scheduler/miui_optimizations
```

---

## 🆘 Comandos de Emergência

### **Modo Seguro**
```bash
# Entrar em modo seguro (desabilita IA)
echo 1 > /sys/kernel/ai_scheduler/safe_mode

# Reset de performance de emergência
echo 1 > /sys/kernel/ai_scheduler/emergency_reset

# Forçar proteção térmica
echo 1 > /sys/kernel/ai_scheduler/force_thermal_protection
```

### **Comandos de Recuperação**
```bash
# Se o sistema ficar sem resposta
echo 1 > /proc/sysrq-trigger  # Habilitar SysRq
echo b > /proc/sysrq-trigger  # Forçar reinicialização

# Resetar IA para padrões de fábrica
echo factory_reset > /sys/kernel/ai_scheduler/reset_mode
```

---

## 📞 Obtendo Ajuda

### **Coleta de Logs**
```bash
# Coletar logs do sistema IA
dmesg | grep ai_scheduler > /sdcard/ai_logs.txt

# Coletar dados de performance
cat /proc/ai_scheduler/debug_info > /sdcard/ai_debug.txt

# Informações do sistema
cat /proc/version > /sdcard/kernel_info.txt
```

### **Canais de Suporte**
- **GitHub Issues**: Reportar bugs e solicitar recursos
- **Telegram**: @BandidoKernel (suporte da comunidade)
- **Thread XDA**: Discussões detalhadas e solução de problemas
- **Discord**: Ajuda da comunidade em tempo real

---

## 🔄 Atualizações

### **Verificando Atualizações**
```bash
# Verificar versão do kernel
cat /proc/version

# Verificar versão do sistema IA
cat /sys/kernel/ai_scheduler/version

# Verificar atualizações disponíveis
cat /sys/kernel/ai_scheduler/update_available
```

### **Processo de Atualização**
1. Baixar o ZIP do kernel mais recente dos releases
2. Inicializar no TWRP
3. Fazer flash do novo ZIP do kernel
4. Reiniciar e verificar funcionalidade

---

**Precisa de mais ajuda? Confira nossa [Wiki](https://github.com/paidefamilia-bot/bandido-sm8250/wiki) ou junte-se à nossa comunidade!**