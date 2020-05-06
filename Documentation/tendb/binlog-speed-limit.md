# TenDB binlog限速
## 背景
为了解决容灾问题，一主多从，或多主多从的方式常常被应用于在MySQL生产环境中。这些容灾方案最重要的一环就是主从复制，增量数据binlog同步。  
在binlog同步时，大多数场景下，我们希望IO线程尽可能快的将binlog同步到Slave节点执行，缩短主从的同步延时; 但在一些特殊而常见的场景下，希望binlog的速度可以被限制： 
1. 一主多从场景下，随着从Slave的线性增长，Master需要将binlog同步给不同的Slave节点，网络带宽随之也线性增长，影响相应请求的速度。  
2. 新增Slave时, (常见于故障后恢复)，一般新增Slave的步骤是导入全备数据，然后再进行增量同步，如果备份时间与重建Slave的时间差较大或Master产生的binlog速度过快，这个时候会存在大量的binlog需要拷贝，可能会造成短暂的网络尖峰，影响应用的正常请求。  
基于以上考虑点，我们希望有一些策略来限制binlog的同步速度，使得Master可以正常工作。

## 如何限速
新增了参数`read_binlog_speed_limit`，表示读binlog的速度限制(单位为KB/s)，作用域为SESSION或GLOBAL，默认为0。(不限制) 此参数适用于Slave，可以动态设定，并且设定之后立即生效。

## 限速如何工作
TenDB通过在Slave拉取行为上实现令牌桶算法来控制拉取速度：  
令牌桶算法的原理大致如下：  
1. 假设有一个桶，容量为n
2. 每秒加入a个令牌
3. 当一个数据包到来时它必须从桶中拿到与数据包大小相同的令牌数才能放行  

速率由a的大小来决定，能通过的最大数据包大小由n决定，原始的令牌桶算法存在丢弃数据包的行为，但是这在我们的场景下是不可接受的。因此将该算法稍作改动：
1. 如果一个event已经到达，那么即使桶满了也不丢弃溢出的令牌，这样无论多大的数据包都能获得足够的令牌数然后被放行;
2. 此外，令牌桶算法要求的每秒加入固定数量的令牌入桶在单线程的模型中也不易实现，因此将其修改成每次写relay_log之前加入令牌。
 
>业界也有其他MySQL发行版支持binlog限速，与TenDB的原理类似，但采用的是Master限速。如果使用Master限速，不需要在每个Slave上都设定限速功能，但是无法做到针对某个具体的Slave限速，不能解决前文提到的新增Slave需求