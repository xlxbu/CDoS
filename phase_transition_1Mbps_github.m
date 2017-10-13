clc;
clear;

route = '/home/nislab/xlx/ns-allinone-3.22-github/ns-3.22';

for i = 1:49
    rho_0(i) = 0.02*i;
    for j = 1:41
        node_id = 82 - j*2;
        data = load(sprintf('%s/CDoS-1Mbps-adhoc-UDP-01/u_0=%0.2frho=0.13R=7T=1500/nodes_%03d_000'...
            ,route, rho_0(i), node_id));
        utilization(i,j) = mean(data(150:end,9));
        data = load(sprintf('%s/CDoS-1Mbps-adhoc-UDP-01/u_0=%0.2frho=0.13R=7T=1500/nodes_%03d_000'...
            ,route, rho_0(i), node_id+1));
        throughput(i,j) = mean(data(150:end,2));
    end
end

figure(1);
%plot(mean(PhyTx0,2)/62.5, mean(PhyTx1,2)/62.5, 'LineStyle', '-', 'Color', 'r','linewidth',6);
%plot(0.02:0.02:0.98, utilization(1,:), 'LineStyle', '-', 'Color', 'r','linewidth',6);
%hold on;
plot(rho_0, utilization(:,20),'LineStyle', '-', 'Color', 'g','linewidth',6);
hold on;
plot(rho_0, utilization(:,40),'LineStyle', ':', 'Color', 'b','linewidth',6);
hold on;
grid on;
xlabel('Load at node $A_0$','FontSize',20,'interpreter','latex');
%xlim([0 1]);
%ylim([0 1]);
%set(gca,'XTick',[10:2:20]);
ylabel('Utilization at node $A_i$','FontSize',20,'interpreter','latex');
h=legend('node $A_{20}$','node $A_{40}$');
set(h,'interpreter','latex');
set(h,'FontSize',20);
set(gcf,'units','centimeters');
set(gcf, 'position', [3 3 16 16.5]);
set(gca,'FontSize',16, 'FontName', 'DejaVuSans');
set(gco,'linewidth',5);
%set(gcf, 'PaperUnits', 'inches');
%set(gcf,'PaperSize',[5 5]);
%set(gca, 'position', 'manual');
set(gca,'units','centimeters');
set(gca,'position',[2.4 2 13 13]);


figure(2);
plot(0:40 ,utilization(10,:),'LineStyle', '-', 'Color', 'r','linewidth',4);
hold on;
plot(0:40, utilization(20,:),'LineStyle', '--', 'Color', 'b','linewidth',4);
hold on;
plot(0:40, utilization(30,:),'LineStyle', '-.', 'Color', 'g','linewidth',4);
hold on;
plot(0:40, utilization(40,:),'LineStyle', ':', 'Color', 'm','linewidth',4);
hold on;
grid on;
xlabel('Node index $i$','FontSize',20,'interpreter','latex');
xlim([0 40]);
ylim([0,0.9]);
%set(gca,'XTick',[10:2:20]);
ylabel('Utilization at node $A_i$','FontSize',20,'interpreter','latex');
h=legend('$\rho_0 = 0.2$','$\rho_0 = 0.4$','$\rho_0 = 0.6$','$\rho_0 = 0.8$');
set(h,'interpreter','latex');
set(h,'FontSize',20);
set(gcf,'units','centimeters');
set(gcf, 'position', [3 3 16 16.5]);
set(gca,'FontSize',16, 'FontName', 'DejaVuSans');
set(gco,'linewidth',5);
%set(gcf, 'PaperUnits', 'inches');
%set(gcf,'PaperSize',[5 5]);
%set(gca, 'position', 'manual');
set(gca,'units','centimeters');
set(gca,'position',[2.4 2 13 13]);