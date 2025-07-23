<#
.SYNOPSIS
Git 仓库管理助手脚本

.DESCRIPTION
提供常用的 Git 仓库管理功能，包括状态显示、初始化、远程库管理、分支操作等
#>

# 主菜单显示
function Show-MainMenu {
    Clear-Host
    Write-Host "================ Git 助手 ================" -ForegroundColor Cyan
    Write-Host "当前状态:"
    Show-CurrentStatus
    
    Write-Host "`n请选择操作:"
    Write-Host "1. 显示当前状态"
    Write-Host "2. Git 仓库初始化"
    Write-Host "3. 显示远程仓库信息"
    Write-Host "4. 切换当前远程库"
    Write-Host "5. 推送到当前跟踪的远程分支"
    Write-Host "6. 创建备份分支"
    Write-Host "7. 合并分支"
    Write-Host "8. 退出"
    Write-Host "==========================================" -ForegroundColor Cyan
}

# 显示当前状态
function Show-CurrentStatus {
    # 1. 当前提交人信息
    $userName = git config user.name
    $userEmail = git config user.email
    Write-Host "提交人  : $userName <$userEmail>"
    
    # 2. 当前分支
    try {
        $branch = git branch --show-current
        if (-not $branch) { $branch = "未在任何分支" }
        Write-Host "当前分支: $branch"
    } catch {
        Write-Host "当前分支: 无法获取" -ForegroundColor Red
    }
    
    # 3. 远程跟踪信息
    try {
        $remoteBranch = git rev-parse --abbrev-ref --symbolic-full-name '@{u}'
        if ($LASTEXITCODE -eq 0) {
            Write-Host "远程跟踪: $remoteBranch"
        } else {
            Write-Host "远程跟踪: 未设置" -ForegroundColor Yellow
        }
    } catch {
        Write-Host "远程跟踪: 错误" -ForegroundColor Red
    }
}

# Git 仓库初始化
function Initialize-GitRepository {
    if (Test-Path .git) {
        Write-Host "错误：当前目录已经是 Git 仓库" -ForegroundColor Red
        return
    }
    
    git init
    Write-Host "`n仓库已初始化" -ForegroundColor Green
    
    # 设置默认用户信息
    $global:userName = Read-Host "输入用户名 (留空跳过)"
    $global:userEmail = Read-Host "输入邮箱 (留空跳过)"
    
    if ($userName) { git config user.name $userName }
    if ($userEmail) { git config user.email $userEmail }
}

# 显示远程仓库信息
function Show-RemoteInfo {
    $remotes = git remote -v | Group-Object { $_ -replace '\s+.*' } | ForEach-Object {
        $remoteName = $_.Name
        $urls = $_.Group -replace '^.*\s+' | Select-Object -Unique
        [PSCustomObject]@{
            Name = $remoteName
            URLs = $urls -join "`n"
        }
    }
    
    if (-not $remotes) {
        Write-Host "未配置任何远程仓库" -ForegroundColor Yellow
        return
    }
    
    Write-Host "`n远程仓库列表:" -ForegroundColor Green
    $remotes | Format-Table @{
        Name = "名称"
        Expression = { $_.Name }
    }, @{
        Name = "地址"
        Expression = { $_.URLs }
    } -AutoSize
}

# 切换当前远程库
function Switch-Remote {
    $remotes = @(git remote)
    if ($remotes.Count -eq 0) {
        Write-Host "未配置任何远程仓库" -ForegroundColor Yellow
        return
    }
    
    Write-Host "`n可用远程仓库:"
    for ($i = 0; $i -lt $remotes.Count; $i++) {
        Write-Host "$($i+1). $($remotes[$i])"
    }
    
    $choice = Read-Host "`n选择要切换的远程仓库 (1-$($remotes.Count))"
    $index = [int]$choice - 1
    
    if ($index -ge 0 -and $index -lt $remotes.Count) {
        $selectedRemote = $remotes[$index]
        $currentBranch = git branch --show-current
        
        if (-not $currentBranch) {
            Write-Host "错误：当前不在任何分支上" -ForegroundColor Red
            return
        }
        
        # 设置上游分支
        git branch --set-upstream-to="$selectedRemote/$currentBranch" $currentBranch
        Write-Host "`n已将分支 '$currentBranch' 的上游设置为 '$selectedRemote/$currentBranch'" -ForegroundColor Green
    } else {
        Write-Host "无效选择" -ForegroundColor Red
    }
}

# 推送到当前跟踪的远程分支
function Push-ToCurrentRemote {
    $currentBranch = git branch --show-current
    if (-not $currentBranch) {
        Write-Host "错误：当前不在任何分支上" -ForegroundColor Red
        return
    }
    
    $remoteInfo = git rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>$null
    if (-not $remoteInfo) {
        Write-Host "错误：当前分支未设置上游分支" -ForegroundColor Red
        return
    }
    
    Write-Host "正在推送 '$currentBranch' 到 '$remoteInfo'..." -ForegroundColor Cyan
    git push
}

# 创建备份分支
function Create-BackupBranch {
    $currentBranch = git branch --show-current
    if (-not $currentBranch) {
        Write-Host "错误：当前不在任何分支上" -ForegroundColor Red
        return
    }
    
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $backupBranch = "${currentBranch}-BACKUP-$timestamp"
    
    git checkout -b $backupBranch
    Write-Host "`n已创建备份分支: $backupBranch" -ForegroundColor Green
    Write-Host "请执行 'git push origin $backupBranch' 推送到远程" -ForegroundColor Cyan
}

# 合并分支
function Merge-Branch {
    $currentBranch = git branch --show-current
    if (-not $currentBranch) {
        Write-Host "错误：当前不在任何分支上" -ForegroundColor Red
        return
    }
    
    $branches = @(git branch --format='%(refname:short)') | Where-Object { $_ -ne $currentBranch }
    if ($branches.Count -eq 0) {
        Write-Host "没有其他分支可合并" -ForegroundColor Yellow
        return
    }
    
    Write-Host "`n当前分支: $currentBranch" -ForegroundColor Cyan
    Write-Host "可选分支:"
    for ($i = 0; $i -lt $branches.Count; $i++) {
        Write-Host "$($i+1). $($branches[$i])"
    }
    
    $choice = Read-Host "`n选择要合并到当前分支的分支 (1-$($branches.Count))"
    $index = [int]$choice - 1
    
    if ($index -ge 0 -and $index -lt $branches.Count) {
        $sourceBranch = $branches[$index]
        Write-Host "正在合并 '$sourceBranch' 到 '$currentBranch'..." -ForegroundColor Cyan
        git merge $sourceBranch --no-ff
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "`n合并成功完成!" -ForegroundColor Green
        } else {
            Write-Host "`n合并过程中出现冲突，请手动解决" -ForegroundColor Red
        }
    } else {
        Write-Host "无效选择" -ForegroundColor Red
    }
}

# 主程序循环
while ($true) {
    Show-MainMenu
    $choice = Read-Host "`n请输入选项 (1-8)"
    
    switch ($choice) {
        '1' { 
            Clear-Host
            Write-Host "====== 当前仓库状态 ======" -ForegroundColor Green
            Show-CurrentStatus
            Read-Host "`n按 Enter 返回主菜单..."
        }
        '2' { 
            Clear-Host
            Write-Host "====== 初始化 Git 仓库 ======" -ForegroundColor Green
            Initialize-GitRepository
            Read-Host "`n按 Enter 返回主菜单..."
        }
        '3' { 
            Clear-Host
            Write-Host "====== 远程仓库信息 ======" -ForegroundColor Green
            Show-RemoteInfo
            Read-Host "`n按 Enter 返回主菜单..."
        }
        '4' { 
            Clear-Host
            Write-Host "====== 切换远程仓库 ======" -ForegroundColor Green
            Switch-Remote
            Read-Host "`n按 Enter 返回主菜单..."
        }
        '5' { 
            Clear-Host
            Write-Host "====== 推送到远程分支 ======" -ForegroundColor Green
            Push-ToCurrentRemote
            Read-Host "`n按 Enter 返回主菜单..."
        }
        '6' { 
            Clear-Host
            Write-Host "====== 创建备份分支 ======" -ForegroundColor Green
            Create-BackupBranch
            Read-Host "`n按 Enter 返回主菜单..."
        }
        '7' { 
            Clear-Host
            Write-Host "====== 合并分支 ======" -ForegroundColor Green
            Merge-Branch
            Read-Host "`n按 Enter 返回主菜单..."
        }
        '8' { exit }
        default {
            Write-Host "无效选择，请重新输入" -ForegroundColor Red
            Start-Sleep -Seconds 1
        }
    }
}